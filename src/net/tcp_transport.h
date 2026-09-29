#pragma once
// Minimal TCP command transport for lockstep multiplayer (ref
// WarsmashServer/WarsmashClient TCP layer, minus the lobby protocol).
//
// Framing: u32 little-endian length + payload (capped at 1 MiB).
// SimCommands travel as SimCommand::kWireSize blobs via SendSimCommand /
// RecvSimCommand. All blocking calls take explicit timeouts so a dead peer
// cannot hang the game thread; sockets stay blocking (no reactor needed
// for turn-rate traffic). POSIX + Winsock portable, no dependencies.
#include <cstdint>
#include <vector>

#include "ecs/lockstep.h"

class TcpTransport {
public:
    static constexpr int kInvalidFd = -1;

    // Must be called once per process on Windows (no-op on POSIX).
    static void GlobalInit();
    static void GlobalShutdown();

    // ---- Server side ----
    // Bind and listen. Port 0 picks an ephemeral port; read it back via
    // BoundPort(). loopbackOnly=false binds INADDR_ANY for LAN play.
    static int Listen(uint16_t port, int backlog = 4, bool loopbackOnly = true);
    static uint16_t BoundPort(int listenFd);
    // Blocking accept with timeout; kInvalidFd on timeout/error.
    static int Accept(int listenFd, int timeoutMs);
    static void Close(int fd);

    // ---- Client side ----
    static int Connect(const char* host, uint16_t port, int timeoutMs);

    // ---- Framing ----
    static bool SendAll(int fd, const uint8_t* data, size_t size);
    static bool RecvAll(int fd, uint8_t* data, size_t size, int timeoutMs);
    static bool SendMessage(int fd, const uint8_t* data, size_t size);
    static bool RecvMessage(int fd, std::vector<uint8_t>& out, int timeoutMs);

    // ---- SimCommand helpers ----
    static bool SendSimCommand(int fd, const SimCommand& cmd);
    static bool RecvSimCommand(int fd, SimCommand& out, int timeoutMs);

    // Non-blocking disconnect probe: true when the peer performed an
    // orderly shutdown (recv==0) or the socket is broken. False when data
    // is available or nothing arrived yet. Note close() is a half-close:
    // sends to a vanished peer may still succeed, so liveness must be
    // judged on the receive side.
    static bool IsPeerGone(int fd);

    // Drain all currently buffered inbound bytes without blocking (used
    // when repurposing a socket, e.g. spectator promotion, where stale
    // broadcasts must not be mistaken for turn submits).
    static void DrainPending(int fd);
};
