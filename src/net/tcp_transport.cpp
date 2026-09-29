#include "tcp_transport.h"

#include <cstring>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
using SocketType = SOCKET;
static constexpr SocketType kBadSocket = INVALID_SOCKET;
static bool WouldBlock() {
    const int err = WSAGetLastError();
    return err == WSAEWOULDBLOCK || err == WSAEINPROGRESS;
}
#else
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
using SocketType = int;
static constexpr SocketType kBadSocket = -1;
static bool WouldBlock() { return errno == EAGAIN || errno == EWOULDBLOCK || errno == EINPROGRESS; }
#endif

namespace {

void CloseSocket(SocketType s) {
#ifdef _WIN32
    closesocket(s);
#else
    ::close(s);
#endif
}

bool WaitFor(int fd, bool write, int timeoutMs) {
    if (timeoutMs < 0) timeoutMs = 0;
    fd_set set;
    FD_ZERO(&set);
    FD_SET((SocketType)fd, &set);
    timeval tv;
    tv.tv_sec = timeoutMs / 1000;
    tv.tv_usec = (timeoutMs % 1000) * 1000;
    const int rc = select((int)fd + 1, write ? nullptr : &set, write ? &set : nullptr,
                          nullptr, &tv);
    return rc > 0 && FD_ISSET((SocketType)fd, &set);
}

void WriteLE32(uint8_t* out, uint32_t v) {
    out[0] = (uint8_t)(v & 0xFF);
    out[1] = (uint8_t)((v >> 8) & 0xFF);
    out[2] = (uint8_t)((v >> 16) & 0xFF);
    out[3] = (uint8_t)((v >> 24) & 0xFF);
}

uint32_t ReadLE32(const uint8_t* in) {
    return (uint32_t)in[0] | ((uint32_t)in[1] << 8) |
           ((uint32_t)in[2] << 16) | ((uint32_t)in[3] << 24);
}

bool SetNonBlocking(SocketType s, bool on) {
#ifdef _WIN32
    u_long mode = on ? 1u : 0u;
    return ioctlsocket(s, FIONBIO, &mode) == 0;
#else
    int flags = fcntl(s, F_GETFL, 0);
    if (flags < 0) return false;
    return fcntl(s, F_SETFL, on ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK)) == 0;
#endif
}

} // namespace

void TcpTransport::GlobalInit() {
#ifdef _WIN32
    WSADATA data;
    WSAStartup(MAKEWORD(2, 2), &data);
#endif
}

void TcpTransport::GlobalShutdown() {
#ifdef _WIN32
    WSACleanup();
#endif
}

int TcpTransport::Listen(uint16_t port, int backlog, bool loopbackOnly) {
    SocketType s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == kBadSocket) return kInvalidFd;
    int reuse = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse));
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(loopbackOnly ? INADDR_LOOPBACK : INADDR_ANY);
    addr.sin_port = htons(port);
    if (bind(s, (const sockaddr*)&addr, sizeof(addr)) != 0) {
        CloseSocket(s);
        return kInvalidFd;
    }
    if (listen(s, backlog) != 0) {
        CloseSocket(s);
        return kInvalidFd;
    }
    return (int)s;
}

uint16_t TcpTransport::BoundPort(int listenFd) {
    sockaddr_in addr{};
    socklen_t len = sizeof(addr);
    if (getsockname((SocketType)listenFd, (sockaddr*)&addr, &len) != 0) return 0;
    return ntohs(addr.sin_port);
}

int TcpTransport::Accept(int listenFd, int timeoutMs) {
    if (!WaitFor(listenFd, false, timeoutMs)) return kInvalidFd;
    const SocketType c = accept((SocketType)listenFd, nullptr, nullptr);
    return c == kBadSocket ? kInvalidFd : (int)c;
}

void TcpTransport::Close(int fd) {
    if (fd != kInvalidFd) CloseSocket((SocketType)fd);
}

int TcpTransport::Connect(const char* host, uint16_t port, int timeoutMs) {
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    char portStr[16];
    snprintf(portStr, sizeof(portStr), "%u", port);
    addrinfo* list = nullptr;
    if (getaddrinfo(host ? host : "127.0.0.1", portStr, &hints, &list) != 0) return kInvalidFd;
    int result = kInvalidFd;
    for (addrinfo* ai = list; ai; ai = ai->ai_next) {
        SocketType s = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (s == kBadSocket) continue;
        if (!SetNonBlocking(s, true)) {
            CloseSocket(s);
            continue;
        }
        int rc = connect(s, ai->ai_addr, (socklen_t)ai->ai_addrlen);
        if (rc != 0 && !WouldBlock()) {
#ifdef _WIN32
            const int err = WSAGetLastError();
            if (err != WSAEWOULDBLOCK && err != WSAEINPROGRESS && err != WSAEALREADY) {
#else
            if (errno != EINPROGRESS) {
#endif
                CloseSocket(s);
                continue;
            }
        }
        if (!WaitFor((int)s, true, timeoutMs)) {
            CloseSocket(s);
            continue;
        }
        int err = 0;
        socklen_t errLen = sizeof(err);
        getsockopt(s, SOL_SOCKET, SO_ERROR, (char*)&err, &errLen);
        if (err != 0) {
            CloseSocket(s);
            continue;
        }
        SetNonBlocking(s, false);
        result = (int)s;
        break;
    }
    freeaddrinfo(list);
    return result;
}

bool TcpTransport::SendAll(int fd, const uint8_t* data, size_t size) {
    size_t sent = 0;
    while (sent < size) {
#ifdef _WIN32
        const int rc = send((SocketType)fd, (const char*)data + sent, (int)(size - sent), 0);
#else
        const ssize_t rc = send((SocketType)fd, data + sent, size - sent, MSG_NOSIGNAL);
#endif
        if (rc <= 0) return false;
        sent += (size_t)rc;
    }
    return true;
}

bool TcpTransport::RecvAll(int fd, uint8_t* data, size_t size, int timeoutMs) {
    size_t got = 0;
    while (got < size) {
        if (!WaitFor(fd, false, timeoutMs)) return false;
#ifdef _WIN32
        const int rc = recv((SocketType)fd, (char*)data + got, (int)(size - got), 0);
#else
        const ssize_t rc = recv((SocketType)fd, data + got, size - got, 0);
#endif
        if (rc <= 0) return false;
        got += (size_t)rc;
    }
    return true;
}

bool TcpTransport::SendMessage(int fd, const uint8_t* data, size_t size) {
    static constexpr size_t kMaxPayload = 1024u * 1024u;
    if (size > kMaxPayload) return false;
    uint8_t header[4];
    WriteLE32(header, (uint32_t)size);
    return SendAll(fd, header, sizeof(header)) && (size == 0 || SendAll(fd, data, size));
}

bool TcpTransport::RecvMessage(int fd, std::vector<uint8_t>& out, int timeoutMs) {
    static constexpr size_t kMaxPayload = 1024u * 1024u;
    uint8_t header[4];
    if (!RecvAll(fd, header, sizeof(header), timeoutMs)) return false;
    const uint32_t size = ReadLE32(header);
    if (size > kMaxPayload) return false;
    out.resize(size);
    return size == 0 || RecvAll(fd, out.data(), size, timeoutMs);
}

bool TcpTransport::SendSimCommand(int fd, const SimCommand& cmd) {
    uint8_t wire[SimCommand::kWireSize];
    cmd.Serialize(wire);
    return SendMessage(fd, wire, sizeof(wire));
}

bool TcpTransport::RecvSimCommand(int fd, SimCommand& out, int timeoutMs) {
    std::vector<uint8_t> payload;
    if (!RecvMessage(fd, payload, timeoutMs)) return false;
    return SimCommand::Deserialize(payload.data(), payload.size(), out);
}

bool TcpTransport::IsPeerGone(int fd) {
    if (fd == kInvalidFd) return true;
    // Only peek when select reports readability, so this never blocks on
    // any platform (a FIN still in flight reads as alive; the next poll
    // observes it).
    if (!WaitFor(fd, false, 0)) return false;
    uint8_t probe = 0;
#ifdef _WIN32
    const int rc = recv((SocketType)fd, (char*)&probe, 1, MSG_PEEK);
    if (rc == 0) return true; // orderly FIN
    if (rc == SOCKET_ERROR) return WSAGetLastError() != WSAEWOULDBLOCK;
    return false; // data waiting: alive
#else
    const ssize_t rc = recv((SocketType)fd, &probe, 1, MSG_PEEK);
    if (rc == 0) return true; // orderly FIN
    if (rc < 0) return errno != EAGAIN && errno != EWOULDBLOCK;
    return false; // data waiting: alive
#endif
}

void TcpTransport::DrainPending(int fd) {
    if (fd == kInvalidFd) return;
    uint8_t chunk[4096];
    while (WaitFor(fd, false, 0)) {
#ifdef _WIN32
        const int rc = recv((SocketType)fd, (char*)chunk, sizeof(chunk), 0);
        if (rc <= 0) break;
#else
        const ssize_t rc = recv((SocketType)fd, chunk, sizeof(chunk), 0);
        if (rc <= 0) break;
#endif
    }
}
