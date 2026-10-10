#include "lockstep_host.h"

#include "net/tcp_transport.h"

LockstepHost::LockstepHost(int numPlayers)
    : manager_(numPlayers),
      peerFds_((size_t)(manager_.NumPlayers() - 1), TcpTransport::kInvalidFd),
      peerMisses_((size_t)(manager_.NumPlayers() - 1), 0) {}

LockstepHost::~LockstepHost() { Close(); }

bool LockstepHost::Listen(uint16_t port, bool loopbackOnly) {
    Close();
    listenFd_ = TcpTransport::Listen(port, 4, loopbackOnly);
    if (listenFd_ == TcpTransport::kInvalidFd) return false;
    // Close() cleared the seat tables; rebuild them for a fresh lobby.
    peerFds_.assign((size_t)(manager_.NumPlayers() - 1), TcpTransport::kInvalidFd);
    peerMisses_.assign((size_t)(manager_.NumPlayers() - 1), 0);
    return true;
}

uint16_t LockstepHost::Port() const {
    if (listenFd_ < 0) return 0;
    return TcpTransport::BoundPort(listenFd_);
}

void LockstepHost::Close() {
    for (int fd : peerFds_) TcpTransport::Close(fd);
    peerFds_.assign((size_t)(manager_.NumPlayers() - 1), TcpTransport::kInvalidFd);
    peerMisses_.assign((size_t)(manager_.NumPlayers() - 1), 0);
    for (int fd : watchers_) TcpTransport::Close(fd);
    watchers_.clear();
    TcpTransport::Close(listenFd_);
    listenFd_ = TcpTransport::kInvalidFd;
}

int LockstepHost::PeerCount() const {
    int n = 0;
    for (int fd : peerFds_) {
        if (fd != TcpTransport::kInvalidFd) n++;
    }
    return n;
}

int LockstepHost::AcceptPeer(int timeoutMs) {
    if (listenFd_ < 0) return -1;
    for (size_t i = 0; i < peerFds_.size(); i++) {
        if (peerFds_[i] != TcpTransport::kInvalidFd) continue;
        const int fd = TcpTransport::Accept(listenFd_, timeoutMs);
        if (fd == TcpTransport::kInvalidFd) return -1;
        peerFds_[i] = fd;
        peerMisses_[i] = 0;
        return (int)i + 1;
    }
    return -1; // lobby full
}

int LockstepHost::AcceptSpectator(int timeoutMs) {
    if (listenFd_ < 0) return -1;
    const int fd = TcpTransport::Accept(listenFd_, timeoutMs);
    if (fd == TcpTransport::kInvalidFd) return -1;
    watchers_.push_back(fd);
    return (int)watchers_.size() - 1;
}

int LockstepHost::WatcherCount() const { return (int)watchers_.size(); }

bool LockstepHost::SendResyncToWatcher(int watcher, const ResyncPackage& pkg) {
    if (watcher < 0 || watcher >= (int)watchers_.size()) return false;
    return SendResync(watchers_[(size_t)watcher], pkg);
}

int LockstepHost::PromoteWatcher(int watcher, int seat) {
    if (watcher < 0 || watcher >= (int)watchers_.size()) return -1;
    const int fd = watchers_[(size_t)watcher];
    // Reap dead sockets first so they never linger, whatever the seat.
    if (TcpTransport::IsPeerGone(fd)) {
        TcpTransport::Close(fd);
        watchers_.erase(watchers_.begin() + watcher);
        return -1;
    }
    if (seat < 1 || seat > manager_.NumPlayers() - 1) return -1;
    const size_t i = (size_t)(seat - 1);
    if (peerFds_[i] != TcpTransport::kInvalidFd) return -1; // occupied
    TcpTransport::DrainPending(fd);
    watchers_.erase(watchers_.begin() + watcher);
    peerFds_[i] = fd;
    peerMisses_[i] = 0;
    manager_.RejoinPlayer(seat); // no-op unless the seat was dropped
    return seat;
}

int LockstepHost::AcceptClaim(int timeoutMs) {    if (listenFd_ < 0) return -1;
    const int fd = TcpTransport::Accept(listenFd_, timeoutMs);
    if (fd == TcpTransport::kInvalidFd) return -1;
    int player = -1;
    bool bound = false;
    if (RecvSeatClaim(fd, player, timeoutMs) && player > 0 &&
        player < manager_.NumPlayers()) {
        const size_t i = (size_t)(player - 1);
        if (peerFds_[i] == TcpTransport::kInvalidFd) {
            peerFds_[i] = fd;
            peerMisses_[i] = 0;
            manager_.RejoinPlayer(player); // no-op unless dropped
            bound = true;
        }
    }
    if (!bound) {
        TcpTransport::Close(fd);
        return -1;
    }
    return player;
}

TurnBatch LockstepHost::RunTurn(std::vector<SimCommand> hostCommands, int timeoutMs,
                                uint64_t stateHash) {
    manager_.SubmitPlayerCommands(0, std::move(hostCommands));
    for (size_t i = 0; i < peerFds_.size(); i++) {
        const int seat = (int)i + 1;
        const int fd = peerFds_[i];
        if (fd == TcpTransport::kInvalidFd) {
            manager_.SubmitPlayerCommands(seat, {});
            continue;
        }
        std::vector<uint8_t> wire;
        TurnBatch peerTurn;
        if (TcpTransport::RecvMessage(fd, wire, timeoutMs) &&
            TurnBatch::Deserialize(wire.data(), wire.size(), peerTurn)) {
            peerMisses_[i] = 0;
            manager_.SubmitPlayerCommands(seat, std::move(peerTurn.commands));
        } else if (TcpTransport::IsPeerGone(fd)) {
            // Orderly shutdown or broken socket: evict at once (a single
            // slow turn must still never evict; that path uses misses).
            TcpTransport::Close(fd);
            peerFds_[i] = TcpTransport::kInvalidFd;
            peerMisses_[i] = 0;
            manager_.DropPlayer(seat);
            manager_.SubmitPlayerCommands(seat, {});
        } else {
            // Pure timeout (peer alive but slow): the seat idles.
            // Persistent timeouts evict it so a wedged socket stops gating
            // the game; orderly shutdowns are evicted at once above.
            peerMisses_[i]++;
            manager_.SubmitPlayerCommands(seat, {});
            if (peerMisses_[i] >= kDropAfterMisses) {
                TcpTransport::Close(fd);
                peerFds_[i] = TcpTransport::kInvalidFd;
                peerMisses_[i] = 0;
                manager_.DropPlayer(seat);
            }
        }
    }
    TurnBatch merged = manager_.TakeBatch();
    merged.stateHash = stateHash;
    auto out = merged.Serialize();
    for (int fd : peerFds_) {
        if (fd == TcpTransport::kInvalidFd) continue;
        TcpTransport::SendMessage(fd, out.data(), out.size());
    }
    // Spectators get the same broadcast best-effort. Drop watchers that
    // are gone (receive-side probe: sends to a vanished peer may still
    // succeed) or whose send fails, so stale watchers never accumulate.
    for (auto it = watchers_.begin(); it != watchers_.end();) {
        if (TcpTransport::IsPeerGone(*it) ||
            !TcpTransport::SendMessage(*it, out.data(), out.size())) {
            TcpTransport::Close(*it);
            it = watchers_.erase(it);
        } else {
            ++it;
        }
    }
    return merged;
}

const std::vector<TurnBatch>& LockstepHost::History() const {
    return manager_.History();
}

void LockstepHost::SeedHistory(std::vector<TurnBatch> history) {
    manager_.SeedFromHistory(std::move(history));
}

bool LockstepHost::SubmitLocalSeatCommands(int seat, std::vector<SimCommand> commands) {
    if (seat == 0) return false; // seat 0 goes through RunTurn's hostCommands
    return manager_.SubmitPlayerCommands(seat, std::move(commands));
}

bool LockstepHost::SendResyncToSeat(int seat, const ResyncPackage& pkg) {
    if (seat < 1 || seat > (int)peerFds_.size()) return false;
    const int fd = peerFds_[(size_t)(seat - 1)];
    if (fd == TcpTransport::kInvalidFd) return false;
    return SendResync(fd, pkg);
}

bool SubmitTurnAndReceive(int fd, const TurnBatch& myTurn, int timeoutMs,
                          TurnBatch& mergedOut) {
    auto out = myTurn.Serialize();
    if (!TcpTransport::SendMessage(fd, out.data(), out.size())) return false;
    std::vector<uint8_t> wire;
    if (!TcpTransport::RecvMessage(fd, wire, timeoutMs)) return false;
    return TurnBatch::Deserialize(wire.data(), wire.size(), mergedOut);
}

bool SendSeatClaim(int fd, int player) {
    uint8_t wire[4];
    wire[0] = (uint8_t)(player & 0xFF);
    wire[1] = (uint8_t)((player >> 8) & 0xFF);
    wire[2] = (uint8_t)((player >> 16) & 0xFF);
    wire[3] = (uint8_t)((player >> 24) & 0xFF);
    return TcpTransport::SendMessage(fd, wire, sizeof(wire));
}

bool RecvSeatClaim(int fd, int& playerOut, int timeoutMs) {
    std::vector<uint8_t> wire;
    if (!TcpTransport::RecvMessage(fd, wire, timeoutMs)) return false;
    if (wire.size() != 4) return false;
    playerOut = (int)((uint32_t)wire[0] | ((uint32_t)wire[1] << 8) |
                      ((uint32_t)wire[2] << 16) | ((uint32_t)wire[3] << 24));
    return true;
}

bool SendResync(int fd, const ResyncPackage& pkg) {
    auto out = pkg.Serialize();
    return TcpTransport::SendMessage(fd, out.data(), out.size());
}

bool RecvResync(int fd, ResyncPackage& pkgOut, int timeoutMs) {
    std::vector<uint8_t> wire;
    if (!TcpTransport::RecvMessage(fd, wire, timeoutMs)) return false;
    return ResyncPackage::Deserialize(wire.data(), wire.size(), pkgOut);
}

int LockstepHost::BroadcastStartGame(const StartGamePackage& pkg) {
    auto out = pkg.Serialize();
    int reached = 0;
    for (int fd : peerFds_) {
        if (fd == TcpTransport::kInvalidFd) continue;
        if (TcpTransport::SendMessage(fd, out.data(), out.size())) reached++;
    }
    for (int fd : watchers_) {
        if (fd != TcpTransport::kInvalidFd) {
            TcpTransport::SendMessage(fd, out.data(), out.size());
        }
    }
    return reached;
}

bool SendStartGame(int fd, const StartGamePackage& pkg) {
    auto out = pkg.Serialize();
    return TcpTransport::SendMessage(fd, out.data(), out.size());
}

bool RecvStartGame(int fd, StartGamePackage& pkgOut, int timeoutMs) {
    std::vector<uint8_t> wire;
    if (!TcpTransport::RecvMessage(fd, wire, timeoutMs)) return false;
    return StartGamePackage::Deserialize(wire.data(), wire.size(), pkgOut);
}
