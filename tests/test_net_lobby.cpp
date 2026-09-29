// Network tests: TCP SimCommand transport over loopback + lobby ready
// model. Sockets stay in-process (127.0.0.1, ephemeral port); every
// blocking call has a timeout so failures fail fast instead of hanging.
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

#include "ecs/lockstep.h"
#include "net/lobby_state.h"
#include "net/tcp_transport.h"

#define TEST_ASSERT(cond) do { if (!(cond)) { std::cerr << "ASSERT @" << __LINE__ << ": " #cond "\n"; return false; } } while (0)

namespace {

SimCommand MakeCmd(int turn, int unit, int order) {
    SimCommand cmd;
    cmd.turn = turn;
    cmd.player = 0;
    cmd.unit = unit;
    cmd.orderId = order;
    cmd.targetKind = 1;
    cmd.targetHandle = -1;
    cmd.x = 10.0f;
    cmd.y = 20.0f;
    cmd.z = 0.0f;
    return cmd;
}

} // namespace

bool TestTcpLoopbackCommandExchange() {
    TcpTransport::GlobalInit();
    const int listenFd = TcpTransport::Listen(0);
    TEST_ASSERT(listenFd != TcpTransport::kInvalidFd);
    const uint16_t port = TcpTransport::BoundPort(listenFd);
    TEST_ASSERT(port != 0);

    // Client connects in the background while we accept with a timeout.
    int clientFd = TcpTransport::kInvalidFd;
    SimCommand clientGot;
    bool clientOk = false;
    std::thread client([&] {
        clientFd = TcpTransport::Connect("127.0.0.1", port, 5000);
        if (clientFd == TcpTransport::kInvalidFd) return;
        // Client -> host (two back-to-back), then host -> client.
        SimCommand back = MakeCmd(9, 77, 851986);
        TcpTransport::SendSimCommand(clientFd, back);
        SimCommand back2 = MakeCmd(9, 78, 851983);
        TcpTransport::SendSimCommand(clientFd, back2);
        clientOk = TcpTransport::RecvSimCommand(clientFd, clientGot, 5000);
    });
    const int hostFd = TcpTransport::Accept(listenFd, 5000);
    TEST_ASSERT(hostFd != TcpTransport::kInvalidFd);

    // Host -> client.
    const SimCommand out = MakeCmd(7, 101, 851986);
    TEST_ASSERT(TcpTransport::SendSimCommand(hostFd, out));
    client.join();
    TEST_ASSERT(clientFd != TcpTransport::kInvalidFd);
    TEST_ASSERT(clientOk);
    TEST_ASSERT(clientGot.unit == 101 && clientGot.turn == 7);

    // Client -> host, twice (framing must split the stream).
    SimCommand in1, in2;
    TEST_ASSERT(TcpTransport::RecvSimCommand(hostFd, in1, 5000));
    TEST_ASSERT(TcpTransport::RecvSimCommand(hostFd, in2, 5000));
    TEST_ASSERT(in1.unit == 77 && in1.orderId == 851986 && in1.turn == 9);
    TEST_ASSERT(in2.unit == 78 && in2.orderId == 851983);
    TEST_ASSERT(in1.x == 10.0f && in1.y == 20.0f);

    TcpTransport::Close(clientFd);
    TcpTransport::Close(hostFd);
    TcpTransport::Close(listenFd);
    TcpTransport::GlobalShutdown();
    return true;
}

bool TestTcpAcceptTimesOut() {
    TcpTransport::GlobalInit();
    const int listenFd = TcpTransport::Listen(0);
    TEST_ASSERT(listenFd != TcpTransport::kInvalidFd);
    // Nobody connects: short timeout must fail fast, not hang.
    const auto start = std::chrono::steady_clock::now();
    TEST_ASSERT(TcpTransport::Accept(listenFd, 200) == TcpTransport::kInvalidFd);
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();
    TEST_ASSERT(ms < 5000);
    // Connecting nowhere fails too.
    TEST_ASSERT(TcpTransport::Connect("127.0.0.1", 1, 300) == TcpTransport::kInvalidFd);
    TcpTransport::Close(listenFd);
    TcpTransport::GlobalShutdown();
    return true;
}

bool TestLobbyReadyGate() {
    LobbyState lobby;
    TEST_ASSERT(lobby.CountUsers() == 0);
    TEST_ASSERT(!lobby.AllUsersReady());
    TEST_ASSERT(!lobby.IsReadyToStart());

    LobbySlot a;
    a.name = "Host";
    a.controller = LobbySlotController::USER;
    a.team = 0;
    a.race = 1;
    a.color = 0;
    a.ready = true;
    TEST_ASSERT(lobby.SetSlot(0, a));
    // One human is not enough for melee start.
    TEST_ASSERT(lobby.AllUsersReady());
    TEST_ASSERT(!lobby.IsReadyToStart());

    LobbySlot b = a;
    b.name = "Guest";
    b.color = 1;
    b.ready = false;
    TEST_ASSERT(lobby.SetSlot(1, b));
    TEST_ASSERT(!lobby.AllUsersReady());
    TEST_ASSERT(!lobby.IsReadyToStart());

    b.ready = true;
    TEST_ASSERT(lobby.SetSlot(1, b));
    TEST_ASSERT(lobby.IsReadyToStart());

    lobby.ResetReady();
    TEST_ASSERT(!lobby.IsReadyToStart());
    return true;
}

bool TestLobbySlotValidation() {
    LobbyState lobby;
    LobbySlot s;
    s.controller = LobbySlotController::USER;
    TEST_ASSERT(!lobby.SetSlot(-1, s));
    TEST_ASSERT(!lobby.SetSlot(12, s));
    s.color = 12;
    TEST_ASSERT(!lobby.SetSlot(0, s));
    s.color = -2;
    TEST_ASSERT(!lobby.SetSlot(0, s));
    s.color = 3;
    s.team = -1;
    TEST_ASSERT(!lobby.SetSlot(0, s));
    s.team = 1;
    s.race = 5;
    TEST_ASSERT(!lobby.SetSlot(0, s));
    s.race = 4;
    TEST_ASSERT(lobby.SetSlot(0, s));
    // Ready on an open seat is cleared.
    LobbySlot open;
    open.ready = true;
    TEST_ASSERT(lobby.SetSlot(2, open));
    TEST_ASSERT(!lobby.GetSlot(2).ready);
    TEST_ASSERT(lobby.GetSlot(0).color == 3);
    return true;
}
