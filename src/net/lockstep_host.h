#pragma once
// Host turn loop (ref GameTurnManager broadcast phase): owns the
// TurnManager plus one TCP peer per remote seat. Each RunTurn gathers
// every peer's command list (timeout or disconnect -> empty submit, the
// seat idles), merges with the host's own list, stamps the host hash and
// broadcasts the merged batch. Peers use SubmitTurnAndReceive.
//
// Seats never shift: peerFds_[i] is always seat i+1 (kInvalidFd when the
// seat is empty or was dropped). A live seat is dropped after
// kDropAfterMisses consecutive receive failures, so one slow turn cannot
// evict a lagging player but a dead socket stops gating the game.
// Cadence is caller-driven (call RunTurn every turn period); no threads.
#include <cstdint>
#include <vector>

#include "ecs/lockstep.h"
#include "net/snapshot_resync.h"
#include "net/turn_batch.h"
#include "net/turn_manager.h"

class LockstepHost {
public:
    static constexpr int kDropAfterMisses = 3;

    // numPlayers counts the host (player 0) plus remote seats.
    explicit LockstepHost(int numPlayers);
    ~LockstepHost();

    LockstepHost(const LockstepHost&) = delete;
    LockstepHost& operator=(const LockstepHost&) = delete;

    bool Listen(uint16_t port, bool loopbackOnly = true);
    uint16_t Port() const;
    void Close();

    // Accept one pending peer with timeout into the first free seat.
    // Returns the seat, or -1 on timeout/error/lobby full.
    int AcceptPeer(int timeoutMs);
    // Accept one pending peer by seat claim (join or rejoin): the socket's
    // first message must be a SeatClaim naming a free or dropped seat
    // (never seat 0). Binds the seat and un-drops it. Returns the seat or -1.
    int AcceptClaim(int timeoutMs);
    int PeerCount() const;
    int Turn() const { return manager_.Turn(); }
    TurnManager& GetTurnManager() { return manager_; }
    const TurnManager& GetTurnManager() const { return manager_; }

    // Spectators (observers, ref uberserver observers): receive-only seats.
    // They never submit, never gate completion, and take no player number.
    // Accepts one pending socket as a spectator; drops it on first failed
    // broadcast. Returns the watcher index, or -1 on timeout/error.
    int AcceptSpectator(int timeoutMs);
    int WatcherCount() const;
    // Catch a (possibly late) spectator up with the full game package.
    bool SendResyncToWatcher(int watcher, const ResyncPackage& pkg);
    // Phase 26: Catch a spectator up in O(1) via compressed keyframe + tail batches.
    bool SendFastKeyframeResyncToWatcher(int watcher, const net::FastResyncKeyframePackage& pkg);

    // Promote a spectator to a player seat (observer -> player): the seat
    // must be free, or dropped (reclaim via claim). Stale broadcasts
    // buffered host-side are drained; the promoted peer must likewise
    // discard its own unread broadcasts (TcpTransport::DrainPending)
    // before receiving the resync, or it will parse one as the package.
    // The peer must then resync (SendResyncToSeat) before live turns.
    // Returns the seat, or -1.
    int PromoteWatcher(int watcher, int seat);

    // Game-start handshake (seed + map checksum distribution): call after
    // the lobby fills and before turn 0. Broadcasts to every bound seat
    // (plus watchers best-effort, like turn batches). Returns seats reached.
    int BroadcastStartGame(const StartGamePackage& pkg);

    // One turn: hostCommands is seat 0's list; stateHash is stamped on the
    // broadcast. Returns the merged batch (also when peers time out).
    TurnBatch RunTurn(std::vector<SimCommand> hostCommands, int timeoutMs,
                      uint64_t stateHash);

    // Merged-batch history for resync packages (see ResyncPackage).
    const std::vector<TurnBatch>& History() const;

    // Seed this host from another host's history (migration): the turn
    // counter continues so peers rejoin mid-game via claim + resync.
    void SeedHistory(std::vector<TurnBatch> history);

    // Submit commands for a locally played seat (host-side AI or a
    // same-process player). Seat 0 keeps going through RunTurn's
    // hostCommands; other seats merge alongside socket peers. False on
    // bad seat / double submit, like TurnManager.
    bool SubmitLocalSeatCommands(int seat, std::vector<SimCommand> commands);

    // Send a resync package to a live seat (after AcceptClaim). False when
    // the seat has no socket or the send fails.
    bool SendResyncToSeat(int seat, const ResyncPackage& pkg);
    // Phase 26: Send a compressed keyframe + tail resync package to a live seat.
    bool SendFastKeyframeResyncToSeat(int seat, const net::FastResyncKeyframePackage& pkg);

private:
    int BroadcastWireToAll(const std::vector<uint8_t>& wire, bool reapWatchers);

    TurnManager manager_;
    int listenFd_ = -1;
    std::vector<int> peerFds_;    // seat i+1, or kInvalidFd when free/dropped
    std::vector<int> peerMisses_; // consecutive receive failures per seat
    std::vector<int> watchers_;   // spectator fds (receive-only, no seats)
};

// Client side of one turn: send my list, receive the merged broadcast.
// False on transport failure (caller keeps its last good batch / idles).
bool SubmitTurnAndReceive(int fd, const TurnBatch& myTurn, int timeoutMs,
                          TurnBatch& mergedOut);

// Seat claim handshake (rejoin or late join): framed LE32 player index.
// The host binds the claimed seat via AcceptClaim; the claimant sends it
// with SendSeatClaim immediately after connecting.
bool SendSeatClaim(int fd, int player);
bool RecvSeatClaim(int fd, int& playerOut, int timeoutMs);

// Resync transfer after a successful claim: host sends FromHistory(seed),
// the rejoiner replays it to the live turn, then normal turns resume.
bool SendResync(int fd, const ResyncPackage& pkg);
bool RecvResync(int fd, ResyncPackage& pkgOut, int timeoutMs);

// Phase 26: Fast keyframe + tail resync transfer over TCP socket.
bool SendFastKeyframeResync(int fd, const net::FastResyncKeyframePackage& pkg);
bool RecvFastKeyframeResync(int fd, net::FastResyncKeyframePackage& pkgOut, int timeoutMs);

// Game-start handshake after the lobby fills (before turn 0): the host
// broadcasts seed + map checksum; peers block on RecvStartGame, verify
// the map hash, and seed their sim. False on transport failure or a
// malformed package (caller stays in the lobby).
bool SendStartGame(int fd, const StartGamePackage& pkg);
bool RecvStartGame(int fd, StartGamePackage& pkgOut, int timeoutMs);
