// Host turn-loop tests: multi-turn host/client exchange, timeout seats,
// lobby-full accept, LAN bind. Loopback only, generous timeouts.
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

#include "data/combat_types.h"
#include "ecs/abilities/ability_move.h"
#include "ecs/csimulation.h"
#include "ecs/combat/combat_system.h"
#include "ecs/lockstep.h"
#include "ecs/player.h"
#include "ecs/unit.h"
#include "ecs/unit_type.h"
#include "net/lockstep_host.h"
#include "net/tcp_transport.h"
#include "net/turn_batch.h"
#include "net/turn_manager.h"

#define TEST_ASSERT(cond) do { if (!(cond)) { std::cerr << "ASSERT @" << __LINE__ << ": " #cond "\n"; return false; } } while (0)

namespace {

SimCommand MakeCmd(int turn, int player, int unit, int order, int kind = 0,
                   int target = -1, float x = 0.0f, float y = 0.0f) {
    SimCommand cmd;
    cmd.turn = turn;
    cmd.player = player;
    cmd.unit = unit;
    cmd.orderId = order;
    cmd.targetKind = kind;
    cmd.targetHandle = target;
    cmd.x = x;
    cmd.y = y;
    cmd.z = 0.0f;
    return cmd;
}

} // namespace

bool TestHostRunsThreeTurns() {
    TcpTransport::GlobalInit();
    LockstepHost host(2);
    TEST_ASSERT(host.Listen(0));
    const uint16_t port = host.Port();
    TEST_ASSERT(port != 0);

    // Client plays seat 1 for three turns.
    std::vector<TurnBatch> seen;
    bool ok = true;
    std::thread peer([&] {
        const int fd = TcpTransport::Connect("127.0.0.1", port, 5000);
        if (fd == TcpTransport::kInvalidFd) {
            ok = false;
            return;
        }
        for (int t = 0; t < 3; t++) {
            TurnBatch mine;
            mine.turn = t;
            mine.commands.push_back(MakeCmd(t, 1, 102, 851972 + t));
            TurnBatch merged;
            if (!SubmitTurnAndReceive(fd, mine, 5000, merged)) {
                ok = false;
                break;
            }
            seen.push_back(merged);
        }
        TcpTransport::Close(fd);
    });

    TEST_ASSERT(host.AcceptPeer(5000) == 1);
    TEST_ASSERT(host.PeerCount() == 1);
    for (int t = 0; t < 3; t++) {
        TurnBatch merged = host.RunTurn({MakeCmd(t, 0, 101, 851986)}, 5000, 1000u + (uint64_t)t);
        TEST_ASSERT(merged.turn == t);
        TEST_ASSERT(merged.commands.size() == 2);
        TEST_ASSERT(merged.commands[0].player == 0);
        TEST_ASSERT(merged.commands[1].player == 1);
        TEST_ASSERT(merged.stateHash == 1000u + (uint64_t)t);
        TEST_ASSERT(host.Turn() == t + 1);
    }
    peer.join();
    TEST_ASSERT(ok);
    TEST_ASSERT(seen.size() == 3);
    for (int t = 0; t < 3; t++) {
        TEST_ASSERT(seen[(size_t)t].turn == t);
        TEST_ASSERT(seen[(size_t)t].commands.size() == 2);
        TEST_ASSERT(seen[(size_t)t].stateHash == 1000u + (uint64_t)t);
    }
    TcpTransport::GlobalShutdown();
    return true;
}

bool TestSilentPeerIdles() {
    TcpTransport::GlobalInit();
    LockstepHost host(2);
    TEST_ASSERT(host.Listen(0));
    const uint16_t port = host.Port();

    // Peer connects but never sends: three consecutive misses evict the
    // seat (slow turns never evict on the first miss, but a wedged socket
    // must stop gating the game). It stays silent past the host's turns.
    std::thread quiet([&] {
        const int fd = TcpTransport::Connect("127.0.0.1", port, 5000);
        if (fd == TcpTransport::kInvalidFd) return;
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
        TcpTransport::Close(fd);
    });

    TEST_ASSERT(host.AcceptPeer(5000) == 1);
    for (int t = 0; t < 3; t++) {
        TurnBatch merged = host.RunTurn({MakeCmd(t, 0, 101, 851986)}, 300, 42u);
        TEST_ASSERT(merged.turn == t);
        TEST_ASSERT(merged.commands.size() == 1); // only the host's
        TEST_ASSERT(merged.commands[0].player == 0);
        if (t < 2) TEST_ASSERT(host.PeerCount() == 1); // misses 1, 2
    }
    TEST_ASSERT(host.PeerCount() == 0); // third miss evicts
    quiet.join();
    TcpTransport::GlobalShutdown();
    return true;
}

bool TestAcceptRejectsWhenFullOrEmpty() {
    TcpTransport::GlobalInit();
    LockstepHost host(1); // host only, no remote seats
    TEST_ASSERT(host.Listen(0));
    TEST_ASSERT(host.AcceptPeer(200) == -1); // lobby full (also nobody there)
    // Solo turns still work.
    TurnBatch merged = host.RunTurn({MakeCmd(0, 0, 101, 851986)}, 200, 7u);
    TEST_ASSERT(merged.commands.size() == 1);
    TEST_ASSERT(host.Turn() == 1);
    TcpTransport::GlobalShutdown();
    return true;
}

bool TestLanBindWorks() {
    TcpTransport::GlobalInit();
    LockstepHost host(2);
    TEST_ASSERT(host.Listen(0, false)); // INADDR_ANY
    TEST_ASSERT(host.Port() != 0);
    TcpTransport::GlobalShutdown();
    return true;
}

namespace {

CUnitType* MakeRejoinType() {
    static std::vector<std::unique_ptr<CUnitType>> keep;
    CUnitAttack atk;
    atk.range = 100.0f;
    atk.cooldown = 1.0f;
    atk.damageBase = 10;
    keep.push_back(std::make_unique<CUnitType>(
        "Test", "Test", "icon", War3ID("hfoo"),
        10000, 1.0f, 0.5f, CRegenType::ALWAYS,
        100, 200, 200, 0, War3ID(),
        std::vector<War3ID>{}, false, PathingGridMovementType::FOOT,
        0.0f, 32.0f, std::vector<std::string>{},
        std::unordered_set<CUnitClassification>{},
        std::vector<CUnitAttack>{atk},
        1, std::string(""), false, true, CDefenseType::NONE,
        0.0f, 1.0f, std::unordered_set<CTargetType>{},
        600.0f, 0.0f, std::vector<War3ID>{}, std::vector<War3ID>{},
        std::vector<War3ID>{}, std::vector<War3ID>{},
        std::unordered_map<CUpgradeClass, War3ID>{}, std::vector<War3ID>{},
        std::vector<War3ID>{}, std::vector<War3ID>{}, CUnitRace{"Human", War3ID("H")},
        100, 50, 1, 0, 30, 0, 0, 0,
        std::unordered_set<CBuildingPathingType>{},
        std::unordered_set<CBuildingPathingType>{},
        0.5f, 0.6f, std::vector<CUnitTypeRequirement>{},
        std::vector<std::vector<CUnitTypeRequirement>>{},
        1, false, 20, 2.0f, 15, 1.5f, 10, 1.0f,
        CPrimaryAttribute::STRENGTH, std::vector<War3ID>{},
        std::vector<std::string>{"Bob"}, 1, false, 1, false, 100,
        0.3f, 0.3f, false, false, 1, 800, 600, false,
        10, 1, 6, 5, 1, 3, false));
    return keep.back().get();
}

void MirrorRejoinUnits(CSimulation& sim) {
    auto* a = sim.AdoptUnit(std::make_unique<CUnit>(401, MakeRejoinType(), 0, War3ID("hfoo"), 0, 0, 0));
    auto* b = sim.AdoptUnit(std::make_unique<CUnit>(402, MakeRejoinType(), 1, War3ID("hfoo"), 0, 500, 0));
    a->AddAbility(sim, std::make_unique<CAbilityMove>(4001));
    b->AddAbility(sim, std::make_unique<CAbilityMove>(4002));
    sim.Update();
}

constexpr int kRejoinSteps = 5;

void StepTurn(CSimulation& sim, const TurnBatch& batch) {
    ApplyBatch(sim, batch);
    for (int i = 0; i < kRejoinSteps; i++) sim.Update();
}

// Client turn split into send/recv halves so the test stays single
// threaded: send first, let the host run, then collect the broadcast.
bool ClientSend(int fd, int turn, std::vector<SimCommand> cmds) {
    TurnBatch mine;
    mine.turn = turn;
    mine.commands = std::move(cmds);
    auto out = mine.Serialize();
    return TcpTransport::SendMessage(fd, out.data(), out.size());
}

bool ClientRecv(int fd, TurnBatch& mergedOut) {
    std::vector<uint8_t> wire;
    if (!TcpTransport::RecvMessage(fd, wire, 5000)) return false;
    return TurnBatch::Deserialize(wire.data(), wire.size(), mergedOut);
}

} // namespace

bool TestClaimValidation() {
    TcpTransport::GlobalInit();
    LockstepHost host(3); // seats 1, 2 free
    TEST_ASSERT(host.Listen(0));
    const uint16_t port = host.Port();
    TEST_ASSERT(host.AcceptClaim(200) == -1); // nobody there

    int fd = TcpTransport::Connect("127.0.0.1", port, 5000);
    TEST_ASSERT(fd != TcpTransport::kInvalidFd);
    TEST_ASSERT(SendSeatClaim(fd, 0)); // seat 0 is the host
    TEST_ASSERT(host.AcceptClaim(5000) == -1);
    TcpTransport::Close(fd);

    fd = TcpTransport::Connect("127.0.0.1", port, 5000);
    TEST_ASSERT(fd != TcpTransport::kInvalidFd);
    TEST_ASSERT(SendSeatClaim(fd, 9)); // out of range
    TEST_ASSERT(host.AcceptClaim(5000) == -1);
    TcpTransport::Close(fd);

    fd = TcpTransport::Connect("127.0.0.1", port, 5000);
    TEST_ASSERT(fd != TcpTransport::kInvalidFd);
    TEST_ASSERT(SendSeatClaim(fd, 2));
    TEST_ASSERT(host.AcceptClaim(5000) == 2); // free seat claim works
    TEST_ASSERT(host.PeerCount() == 1);

    int fd2 = TcpTransport::Connect("127.0.0.1", port, 5000);
    TEST_ASSERT(fd2 != TcpTransport::kInvalidFd);
    TEST_ASSERT(SendSeatClaim(fd2, 2)); // occupied now
    TEST_ASSERT(host.AcceptClaim(5000) == -1);
    TcpTransport::Close(fd);
    TcpTransport::Close(fd2);
    TcpTransport::GlobalShutdown();
    return true;
}

bool TestRejoinCatchesUp() {
    TcpTransport::GlobalInit();
    constexpr uint32_t kSeed = 777001u;
    CSimulation hostSim;
    hostSim.SeedRng(kSeed);
    MirrorRejoinUnits(hostSim);

    LockstepHost host(2);
    TEST_ASSERT(host.Listen(0));
    const uint16_t port = host.Port();

    // Turn 0 through the live host with peer A aboard.
    int fdA = TcpTransport::Connect("127.0.0.1", port, 5000);
    TEST_ASSERT(fdA != TcpTransport::kInvalidFd);
    TEST_ASSERT(host.AcceptPeer(5000) == 1);
    TEST_ASSERT(ClientSend(fdA, 0, {MakeCmd(0, 1, 402, 851986, 1, -1, 200, 500)}));
    TurnBatch t0 = host.RunTurn({MakeCmd(0, 0, 401, 851986, 1, -1, 200, 0)}, 5000, hostSim.HashState());
    TurnBatch seen0;
    TEST_ASSERT(ClientRecv(fdA, seen0));
    TEST_ASSERT(seen0.commands.size() == 2);
    StepTurn(hostSim, t0);
    TcpTransport::Close(fdA); // peer A vanishes

    // Turns 1..3 solo; the dead seat is evicted on the third miss.
    for (int t = 1; t <= 3; t++) {
        TurnBatch merged = host.RunTurn({MakeCmd(t, 0, 401, 851986, 1, -1, 200 + 100 * t, 0)},
                                        300, hostSim.HashState());
        StepTurn(hostSim, merged);
    }
    TEST_ASSERT(host.PeerCount() == 0);
    TEST_ASSERT(host.Turn() == 4);

    // Rejoin as seat 1 with a fresh socket + claim, then take the package.
    int fdB = TcpTransport::Connect("127.0.0.1", port, 5000);
    TEST_ASSERT(fdB != TcpTransport::kInvalidFd);
    TEST_ASSERT(SendSeatClaim(fdB, 1));
    TEST_ASSERT(host.AcceptClaim(5000) == 1);
    TEST_ASSERT(host.PeerCount() == 1);
    ResyncPackage pkg = ResyncPackage::FromHistory(kSeed, host.History());
    TEST_ASSERT(host.SendResyncToSeat(1, pkg));
    ResyncPackage got;
    TEST_ASSERT(RecvResync(fdB, got, 5000));
    TEST_ASSERT(got.seed == kSeed);
    TEST_ASSERT(got.commands.size() == 2 + 1 + 1 + 1); // turns 0..3

    // Fresh sim replays to the live turn.
    CSimulation caught;
    caught.SeedRng(got.seed);
    MirrorRejoinUnits(caught);
    for (int t = 0; t <= 3; t++) {
        TurnBatch rb;
        rb.turn = t;
        for (const auto& c : got.commands) {
            if (c.turn == t) rb.commands.push_back(c);
        }
        StepTurn(caught, rb);
    }
    TEST_ASSERT(caught.HashState() == hostSim.HashState());

    // And the rebound seat plays live turns again.
    TEST_ASSERT(ClientSend(fdB, 4, {MakeCmd(4, 1, 402, 851986, 1, -1, 600, 500)}));
    TurnBatch t4 = host.RunTurn({MakeCmd(4, 0, 401, 851986, 1, -1, 600, 0)}, 5000,
                                hostSim.HashState());
    TurnBatch seen4;
    TEST_ASSERT(ClientRecv(fdB, seen4));
    TEST_ASSERT(seen4.commands.size() == 2);
    StepTurn(hostSim, t4);
    StepTurn(caught, t4);
    TEST_ASSERT(caught.HashState() == hostSim.HashState());

    TcpTransport::Close(fdB);
    TcpTransport::GlobalShutdown();
    return true;
}

bool TestRejoinPlayerLogic() {    TurnManager mgr(2);
    TEST_ASSERT(!mgr.RejoinPlayer(0)); // not dropped
    TEST_ASSERT(!mgr.RejoinPlayer(9));
    TEST_ASSERT(mgr.DropPlayer(1));
    TEST_ASSERT(mgr.SubmitPlayerCommands(0, {MakeCmd(0, 0, 101, 851986)}));
    TEST_ASSERT(mgr.IsComplete()); // dropped seat doesn't gate
    mgr.TakeBatch();
    TEST_ASSERT(mgr.RejoinPlayer(1));
    TEST_ASSERT(!mgr.IsDropped(1));
    TEST_ASSERT(!mgr.IsComplete()); // gates again
    TEST_ASSERT(mgr.SubmitPlayerCommands(0, {}));
    TEST_ASSERT(mgr.SubmitPlayerCommands(1, {}));
    TEST_ASSERT(mgr.IsComplete());
    return true;
}

namespace {

CUnitType* MakeResyncType() {
    static std::vector<std::unique_ptr<CUnitType>> keep;
    CUnitAttack atk;
    atk.range = 100.0f;
    atk.cooldown = 1.0f;
    atk.damageBase = 10;
    keep.push_back(std::make_unique<CUnitType>(
        "Test", "Test", "icon", War3ID("hfoo"),
        10000, 1.0f, 0.5f, CRegenType::ALWAYS,
        100, 200, 200, 0, War3ID(),
        std::vector<War3ID>{}, false, PathingGridMovementType::FOOT,
        0.0f, 32.0f, std::vector<std::string>{},
        std::unordered_set<CUnitClassification>{},
        std::vector<CUnitAttack>{atk},
        1, std::string(""), false, true, CDefenseType::NONE,
        0.0f, 1.0f, std::unordered_set<CTargetType>{},
        600.0f, 0.0f, std::vector<War3ID>{}, std::vector<War3ID>{},
        std::vector<War3ID>{}, std::vector<War3ID>{},
        std::unordered_map<CUpgradeClass, War3ID>{}, std::vector<War3ID>{},
        std::vector<War3ID>{}, std::vector<War3ID>{}, CUnitRace{"Human", War3ID("H")},
        100, 50, 1, 0, 30, 0, 0, 0,
        std::unordered_set<CBuildingPathingType>{},
        std::unordered_set<CBuildingPathingType>{},
        0.5f, 0.6f, std::vector<CUnitTypeRequirement>{},
        std::vector<std::vector<CUnitTypeRequirement>>{},
        1, false, 20, 2.0f, 15, 1.5f, 10, 1.0f,
        CPrimaryAttribute::STRENGTH, std::vector<War3ID>{},
        std::vector<std::string>{"Bob"}, 1, false, 1, false, 100,
        0.3f, 0.3f, false, false, 1, 800, 600, false,
        10, 1, 6, 5, 1, 3, false));
    return keep.back().get();
}

void MirrorResyncUnits(CSimulation& sim) {
    auto* a = sim.AdoptUnit(std::make_unique<CUnit>(301, MakeResyncType(), 0, War3ID("hfoo"), 0, 0, 0));
    auto* b = sim.AdoptUnit(std::make_unique<CUnit>(302, MakeResyncType(), 1, War3ID("hfoo"), 0, 500, 0));
    a->AddAbility(sim, std::make_unique<CAbilityMove>(3001));
    b->AddAbility(sim, std::make_unique<CAbilityMove>(3002));
    sim.Update();
}

// One live turn: apply ancestral commands, roll dice damage both ways,
// heal back, step. Returns the damage dealt this turn (RNG-sensitive).
float LiveTurn(CSimulation& sim, const std::vector<SimCommand>& cmds) {
    TurnBatch batch;
    batch.commands = cmds;
    ApplyBatch(sim, batch);
    float total = 0.0f;
    auto* a = sim.GetUnit(301);
    auto* b = sim.GetUnit(302);
    for (int i = 0; i < 4; i++) {
        DamageResult r = sim.GetCombatSystem()->CalculateDamage(
            a, b, true, false, CAttackType::NORMAL, CDamageType::NORMAL, 10, 0, 2, 6);
        total += r.damageDealt;
        b->SetLife(sim, 10000.0f);
    }
    for (int i = 0; i < 5; i++) sim.Update();
    return total;
}

} // namespace

bool TestResyncPackageRoundTrip() {
    TurnManager mgr(2);
    mgr.SubmitPlayerCommands(0, {MakeCmd(0, 0, 101, 851986)});
    mgr.SubmitPlayerCommands(1, {MakeCmd(0, 1, 102, 851972)});
    mgr.TakeBatch();
    mgr.SubmitPlayerCommands(0, {});
    mgr.SubmitPlayerCommands(1, {MakeCmd(1, 1, 102, 851986)});
    mgr.TakeBatch();
    TEST_ASSERT(mgr.History().size() == 2);

    ResyncPackage pkg = ResyncPackage::FromHistory(555u, mgr.History());
    TEST_ASSERT(pkg.seed == 555u);
    TEST_ASSERT(pkg.commands.size() == 3);
    auto wire = pkg.Serialize();
    ResyncPackage back;
    TEST_ASSERT(ResyncPackage::Deserialize(wire.data(), wire.size(), back));
    TEST_ASSERT(back.seed == 555u);
    TEST_ASSERT(back.commands.size() == 3);
    TEST_ASSERT(back.commands[0].unit == 101 && back.commands[2].turn == 1);
    TEST_ASSERT(!ResyncPackage::Deserialize(wire.data(), wire.size() - 1, back));
    TEST_ASSERT(!ResyncPackage::Deserialize(wire.data(), 3, back));
    return true;
}

bool TestReplayLandsOnLiveHash() {
    constexpr uint32_t kSeed = 20240u;
    // Live game: 3 turns of moves + dice.
    CSimulation live;
    live.SeedRng(kSeed);
    MirrorResyncUnits(live);
    std::vector<std::vector<SimCommand>> turns = {
        {MakeCmd(0, 0, 301, 851986, 1, -1, 200, 0), MakeCmd(0, 1, 302, 851986, 1, -1, 200, 500)},
        {MakeCmd(1, 0, 301, 851986, 1, -1, 400, 0)},
        {},
    };
    std::vector<float> liveDamage;
    for (const auto& cmds : turns) liveDamage.push_back(LiveTurn(live, cmds));
    const uint64_t liveHash = live.HashState();

    // Package the game, then replay it on a fresh sim.
    ResyncPackage pkg;
    pkg.seed = kSeed;
    for (const auto& cmds : turns) {
        for (const auto& c : cmds) pkg.commands.push_back(c);
    }
    auto wire = pkg.Serialize();
    ResyncPackage got;
    TEST_ASSERT(ResyncPackage::Deserialize(wire.data(), wire.size(), got));

    CSimulation fresh;
    fresh.SeedRng(got.seed);
    MirrorResyncUnits(fresh);
    std::vector<float> replayDamage;
    for (const auto& cmds : turns) replayDamage.push_back(LiveTurn(fresh, cmds));
    TEST_ASSERT(replayDamage == liveDamage);
    TEST_ASSERT(fresh.HashState() == liveHash);
    return true;
}

bool TestHostHistoryGrows() {
    TcpTransport::GlobalInit();
    LockstepHost host(1);
    TEST_ASSERT(host.Listen(0));
    TEST_ASSERT(host.History().empty());
    host.RunTurn({MakeCmd(0, 0, 101, 851986)}, 100, 1u);
    host.RunTurn({}, 100, 2u);
    TEST_ASSERT(host.History().size() == 2);
    TEST_ASSERT(host.History()[0].turn == 0);
    TEST_ASSERT(host.History()[1].turn == 1);
    TEST_ASSERT(host.History()[0].commands.size() == 1);
    TEST_ASSERT(host.History()[1].commands.empty());
    TcpTransport::GlobalShutdown();
    return true;
}

bool TestTurnManagerDropUnblocks() {
    TurnManager mgr(3);
    TEST_ASSERT(!mgr.DropPlayer(9));
    TEST_ASSERT(mgr.DropPlayer(1));
    TEST_ASSERT(mgr.IsDropped(1));
    TEST_ASSERT(!mgr.SubmitPlayerCommands(1, {})); // dropped seats reject
    TEST_ASSERT(!mgr.IsComplete());
    TEST_ASSERT(mgr.SubmitPlayerCommands(0, {MakeCmd(0, 0, 101, 851986)}));
    TEST_ASSERT(!mgr.IsComplete());
    TEST_ASSERT(mgr.SubmitPlayerCommands(2, {MakeCmd(0, 2, 103, 851972)}));
    TEST_ASSERT(mgr.IsComplete());
    TurnBatch batch = mgr.TakeBatch();
    TEST_ASSERT(batch.commands.size() == 2);
    TEST_ASSERT(batch.commands[0].player == 0);
    TEST_ASSERT(batch.commands[1].player == 2);
    return true;
}

bool TestDisconnectEvictsSeat() {
    TcpTransport::GlobalInit();
    LockstepHost host(2);
    TEST_ASSERT(host.Listen(0));
    const uint16_t port = host.Port();

    // Client plays one turn, then vanishes mid-game.
    std::thread peer([&] {
        const int fd = TcpTransport::Connect("127.0.0.1", port, 5000);
        if (fd == TcpTransport::kInvalidFd) return;
        TurnBatch mine;
        mine.turn = 0;
        mine.commands.push_back(MakeCmd(0, 1, 102, 851972));
        TurnBatch merged;
        SubmitTurnAndReceive(fd, mine, 5000, merged);
        TcpTransport::Close(fd); // disconnect without goodbye
    });

    TEST_ASSERT(host.AcceptPeer(5000) == 1);
    TurnBatch first = host.RunTurn({MakeCmd(0, 0, 101, 851986)}, 5000, 1u);
    TEST_ASSERT(first.commands.size() == 2);
    peer.join();

    // The orderly shutdown is evicted on the very next turn (receive-side
    // FIN probe); the game continues on the host's commands alone.
    TurnBatch merged = host.RunTurn({MakeCmd(1, 0, 101, 851986)}, 300, 101u);
    TEST_ASSERT(merged.turn == 1);
    TEST_ASSERT(merged.commands.size() == 1);
    TEST_ASSERT(merged.commands[0].player == 0);
    TEST_ASSERT(host.PeerCount() == 0);
    TEST_ASSERT(host.Turn() == 2);
    // Post-eviction turns don't wait on anyone.
    TurnBatch solo = host.RunTurn({}, 300, 999u);
    TEST_ASSERT(solo.turn == 2);
    TEST_ASSERT(solo.commands.empty());
    TcpTransport::GlobalShutdown();
    return true;
}

namespace {

void MirrorMigrationUnits(CSimulation& sim) {
    auto* a = sim.AdoptUnit(std::make_unique<CUnit>(501, MakeRejoinType(), 1, War3ID("hfoo"), 0, 0, 0));
    auto* b = sim.AdoptUnit(std::make_unique<CUnit>(502, MakeRejoinType(), 2, War3ID("hfoo"), 0, 500, 0));
    a->AddAbility(sim, std::make_unique<CAbilityMove>(5001));
    b->AddAbility(sim, std::make_unique<CAbilityMove>(5002));
    sim.Update();
}

} // namespace

bool TestSeedFromHistoryLogic() {
    TurnManager mgr(2);
    mgr.SubmitPlayerCommands(0, {MakeCmd(0, 0, 101, 851986)});
    mgr.SubmitPlayerCommands(1, {});
    mgr.TakeBatch();
    mgr.SubmitPlayerCommands(0, {});
    mgr.SubmitPlayerCommands(1, {MakeCmd(1, 1, 102, 851972)});
    mgr.TakeBatch();
    TEST_ASSERT(mgr.Turn() == 2);

    TurnManager fresh(2);
    TEST_ASSERT(fresh.Turn() == 0);
    fresh.SeedFromHistory(mgr.History());
    TEST_ASSERT(fresh.Turn() == 2);
    TEST_ASSERT(fresh.History().size() == 2);
    TEST_ASSERT(fresh.History()[1].commands.size() == 1);
    // Live turns continue on top.
    TEST_ASSERT(fresh.SubmitPlayerCommands(0, {MakeCmd(2, 0, 101, 851986)}));
    TEST_ASSERT(fresh.SubmitPlayerCommands(1, {}));
    TurnBatch next = fresh.TakeBatch();
    TEST_ASSERT(next.turn == 2);
    TEST_ASSERT(fresh.History().size() == 3);
    return true;
}

bool TestHostMigrationContinues() {
    TcpTransport::GlobalInit();
    constexpr uint32_t kSeed = 414141u;
    CSimulation simA, simB;
    simA.SeedRng(kSeed);
    simB.SeedRng(kSeed);
    MirrorMigrationUnits(simA);
    MirrorMigrationUnits(simB);

    // Seatless original host + two players, one live turn.
    LockstepHost oldHost(3);
    TEST_ASSERT(oldHost.Listen(0));
    int fdA = TcpTransport::Connect("127.0.0.1", oldHost.Port(), 5000);
    int fdB = TcpTransport::Connect("127.0.0.1", oldHost.Port(), 5000);
    TEST_ASSERT(fdA != TcpTransport::kInvalidFd && fdB != TcpTransport::kInvalidFd);
    TEST_ASSERT(oldHost.AcceptPeer(5000) == 1);
    TEST_ASSERT(oldHost.AcceptPeer(5000) == 2);
    TEST_ASSERT(ClientSend(fdA, 0, {MakeCmd(0, 1, 501, 851986, 1, -1, 200, 0)}));
    TEST_ASSERT(ClientSend(fdB, 0, {MakeCmd(0, 2, 502, 851986, 1, -1, 200, 500)}));
    TurnBatch t0 = oldHost.RunTurn({}, 5000, 11u);
    TurnBatch seenA, seenB;
    TEST_ASSERT(ClientRecv(fdA, seenA) && ClientRecv(fdB, seenB));
    TEST_ASSERT(seenA.commands.size() == 2 && seenB.commands.size() == 2);
    StepTurn(simA, t0);
    StepTurn(simB, t0);
    TEST_ASSERT(simA.HashState() == simB.HashState());
    std::vector<TurnBatch> saved = {t0}; // survivor A's copy of history
    oldHost.Close(); // the host dies here
    TcpTransport::Close(fdA);
    TcpTransport::Close(fdB);

    // Survivor process starts a new host seeded from history.
    LockstepHost newHost(3);
    newHost.SeedHistory(saved);
    TEST_ASSERT(newHost.Turn() == 1);
    TEST_ASSERT(newHost.History().size() == 1);
    TEST_ASSERT(newHost.Listen(0));

    // B reclaims seat 2 and resyncs onto a fresh sim.
    fdB = TcpTransport::Connect("127.0.0.1", newHost.Port(), 5000);
    TEST_ASSERT(fdB != TcpTransport::kInvalidFd);
    TEST_ASSERT(SendSeatClaim(fdB, 2));
    TEST_ASSERT(newHost.AcceptClaim(5000) == 2);
    TEST_ASSERT(newHost.SendResyncToSeat(2, ResyncPackage::FromHistory(kSeed, newHost.History())));
    ResyncPackage got;
    TEST_ASSERT(RecvResync(fdB, got, 5000));
    CSimulation simB2;
    simB2.SeedRng(got.seed);
    MirrorMigrationUnits(simB2);
    // Replay grouped by turn: one batch + one step window per turn,
    // exactly like the live game above.
    int replayTurns = 0;
    for (const auto& h : newHost.History()) replayTurns = std::max(replayTurns, h.turn + 1);
    for (int t = 0; t < replayTurns; t++) {
        TurnBatch rb;
        rb.turn = t;
        for (const auto& c : got.commands) {
            if (c.turn == t) rb.commands.push_back(c);
        }
        StepTurn(simB2, rb);
    }
    TEST_ASSERT(simB2.HashState() == simA.HashState());

    // A reclaims seat 1; turn 1 runs on the migrated host, A via socket.
    fdA = TcpTransport::Connect("127.0.0.1", newHost.Port(), 5000);
    TEST_ASSERT(fdA != TcpTransport::kInvalidFd);
    TEST_ASSERT(SendSeatClaim(fdA, 1));
    TEST_ASSERT(newHost.AcceptClaim(5000) == 1);
    TEST_ASSERT(newHost.PeerCount() == 2);
    TEST_ASSERT(ClientSend(fdA, 1, {MakeCmd(1, 1, 501, 851986, 1, -1, 400, 0)}));
    TEST_ASSERT(ClientSend(fdB, 1, {MakeCmd(1, 2, 502, 851986, 1, -1, 400, 500)}));
    TurnBatch t1 = newHost.RunTurn({}, 5000, 22u);
    TEST_ASSERT(t1.turn == 1 && t1.commands.size() == 2);
    TEST_ASSERT(ClientRecv(fdA, seenA) && ClientRecv(fdB, seenB));
    StepTurn(simA, t1);
    StepTurn(simB2, t1);
    TEST_ASSERT(simA.HashState() == simB2.HashState());

    // Turn 2 exercises local-seat submit: A is driven in-process.
    TEST_ASSERT(newHost.SubmitLocalSeatCommands(1, {MakeCmd(2, 1, 501, 851972)}));
    TEST_ASSERT(!newHost.SubmitLocalSeatCommands(1, {})); // double submit
    TEST_ASSERT(!newHost.SubmitLocalSeatCommands(0, {})); // seat 0 reserved
    TEST_ASSERT(!newHost.SubmitLocalSeatCommands(9, {})); // bad seat
    TEST_ASSERT(ClientSend(fdB, 2, {MakeCmd(2, 2, 502, 851972)}));
    TurnBatch t2 = newHost.RunTurn({}, 5000, 33u);
    TEST_ASSERT(t2.turn == 2 && t2.commands.size() == 2);
    TEST_ASSERT(ClientRecv(fdB, seenB));
    StepTurn(simA, t2);
    StepTurn(simB2, t2);
    TEST_ASSERT(simA.HashState() == simB2.HashState());

    TcpTransport::Close(fdA);
    TcpTransport::Close(fdB);
    TcpTransport::GlobalShutdown();
    return true;
}

bool TestSpectatorReceivesTurns() {
    TcpTransport::GlobalInit();
    LockstepHost host(2);
    TEST_ASSERT(host.Listen(0));
    const uint16_t port = host.Port();

    int fdP = TcpTransport::Connect("127.0.0.1", port, 5000);
    TEST_ASSERT(fdP != TcpTransport::kInvalidFd);
    TEST_ASSERT(host.AcceptPeer(5000) == 1);
    int fdW = TcpTransport::Connect("127.0.0.1", port, 5000);
    TEST_ASSERT(fdW != TcpTransport::kInvalidFd);
    TEST_ASSERT(host.AcceptSpectator(5000) == 0);
    TEST_ASSERT(host.WatcherCount() == 1);
    TEST_ASSERT(host.AcceptSpectator(200) == -1); // nobody else waiting

    // The spectator sends nothing at all; turns must still complete.
    for (int t = 0; t < 2; t++) {
        TEST_ASSERT(ClientSend(fdP, t, {MakeCmd(t, 1, 102, 851972)}));
        TurnBatch merged = host.RunTurn({MakeCmd(t, 0, 101, 851986)}, 5000, 5u);
        TEST_ASSERT(merged.turn == t && merged.commands.size() == 2);
        TurnBatch seenP, seenW;
        TEST_ASSERT(ClientRecv(fdP, seenP));
        std::vector<uint8_t> wire;
        TEST_ASSERT(TcpTransport::RecvMessage(fdW, wire, 5000));
        TEST_ASSERT(TurnBatch::Deserialize(wire.data(), wire.size(), seenW));
        TEST_ASSERT(seenW.turn == t && seenW.commands.size() == 2);
        TEST_ASSERT(seenW.commands == seenP.commands);
    }
    TcpTransport::Close(fdP);
    TcpTransport::Close(fdW);
    TcpTransport::GlobalShutdown();
    return true;
}

bool TestSpectatorDropOnSendFailure() {
    TcpTransport::GlobalInit();
    LockstepHost host(1); // solo host, spectator only
    TEST_ASSERT(host.Listen(0));
    const uint16_t port = host.Port();
    int fdW = TcpTransport::Connect("127.0.0.1", port, 5000);
    TEST_ASSERT(fdW != TcpTransport::kInvalidFd);
    TEST_ASSERT(host.AcceptSpectator(5000) == 0);
    TEST_ASSERT(host.WatcherCount() == 1);
    TcpTransport::Close(fdW); // spectator vanishes
    // The receive-side FIN probe drops it on the next broadcast (sends
    // alone cannot observe the half-close). Brief sleep so the FIN lands;
    // any later turn would drop it regardless.
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    TurnBatch merged = host.RunTurn({MakeCmd(0, 0, 101, 851986)}, 500, 9u);
    TEST_ASSERT(merged.commands.size() == 1);
    TEST_ASSERT(host.WatcherCount() == 0); // dropped on failed broadcast
    TEST_ASSERT(!host.SendResyncToWatcher(0, ResyncPackage{})); // bad index
    TcpTransport::GlobalShutdown();
    return true;
}

bool TestLateSpectatorResync() {
    TcpTransport::GlobalInit();
    constexpr uint32_t kSeed = 60606u;
    CSimulation live;
    live.SeedRng(kSeed);
    MirrorRejoinUnits(live);

    LockstepHost host(1); // solo host game, two turns in
    TEST_ASSERT(host.Listen(0));
    for (int t = 0; t < 2; t++) {
        TurnBatch merged = host.RunTurn({MakeCmd(t, 0, 401, 851986, 1, -1, 200 + 100 * t, 0)},
                                        500, live.HashState());
        StepTurn(live, merged);
    }
    TEST_ASSERT(host.History().size() == 2);

    // Spectator arrives late and catches up from the package.
    int fdW = TcpTransport::Connect("127.0.0.1", host.Port(), 5000);
    TEST_ASSERT(fdW != TcpTransport::kInvalidFd);
    TEST_ASSERT(host.AcceptSpectator(5000) == 0);
    ResyncPackage pkg = ResyncPackage::FromHistory(kSeed, host.History());
    TEST_ASSERT(host.SendResyncToWatcher(0, pkg));
    ResyncPackage got;
    TEST_ASSERT(RecvResync(fdW, got, 5000));
    CSimulation caught;
    caught.SeedRng(got.seed);
    MirrorRejoinUnits(caught);
    for (int t = 0; t < 2; t++) {
        TurnBatch rb;
        rb.turn = t;
        for (const auto& c : got.commands) {
            if (c.turn == t) rb.commands.push_back(c);
        }
        StepTurn(caught, rb);
    }
    TEST_ASSERT(caught.HashState() == live.HashState());
    TcpTransport::Close(fdW);
    TcpTransport::GlobalShutdown();
    return true;
}

namespace {

void MirrorPromoteUnits(CSimulation& sim) {
    auto* a = sim.AdoptUnit(std::make_unique<CUnit>(601, MakeRejoinType(), 1, War3ID("hfoo"), 0, 0, 0));
    auto* b = sim.AdoptUnit(std::make_unique<CUnit>(602, MakeRejoinType(), 2, War3ID("hfoo"), 0, 500, 0));
    a->AddAbility(sim, std::make_unique<CAbilityMove>(6001));
    b->AddAbility(sim, std::make_unique<CAbilityMove>(6002));
    sim.Update();
}

} // namespace

bool TestPromoteWatcherPlays() {
    TcpTransport::GlobalInit();
    constexpr uint32_t kSeed = 80808u;
    CSimulation simLive;
    simLive.SeedRng(kSeed);
    MirrorPromoteUnits(simLive);

    LockstepHost host(3); // seatless host, seats 1-2 for players
    TEST_ASSERT(host.Listen(0));
    const uint16_t port = host.Port();
    int fdP = TcpTransport::Connect("127.0.0.1", port, 5000);
    TEST_ASSERT(fdP != TcpTransport::kInvalidFd);
    TEST_ASSERT(host.AcceptPeer(5000) == 1);
    int fdW = TcpTransport::Connect("127.0.0.1", port, 5000);
    TEST_ASSERT(fdW != TcpTransport::kInvalidFd);
    TEST_ASSERT(host.AcceptSpectator(5000) == 0);

    // Turn 0 with one player; the spectator watches but never reads (the
    // promotion must drain its stale broadcast itself).
    TEST_ASSERT(ClientSend(fdP, 0, {MakeCmd(0, 1, 601, 851986, 1, -1, 200, 0)}));
    TurnBatch t0 = host.RunTurn({}, 5000, simLive.HashState());
    TurnBatch seenP;
    TEST_ASSERT(ClientRecv(fdP, seenP));
    TEST_ASSERT(seenP.commands.size() == 1);
    StepTurn(simLive, t0);

    // Promote the spectator to seat 2, resync it, replay to live.
    TEST_ASSERT(host.PromoteWatcher(0, 2) == 2);
    TEST_ASSERT(host.PeerCount() == 2); // seat 1 player + promoted seat 2
    TEST_ASSERT(host.WatcherCount() == 0);
    // The peer side still holds the unread turn-0 broadcast: discard it
    // before the resync (host-side drain cannot reach peer buffers).
    TcpTransport::DrainPending(fdW);
    ResyncPackage pkg = ResyncPackage::FromHistory(kSeed, host.History());
    TEST_ASSERT(host.SendResyncToSeat(2, pkg));
    ResyncPackage got;
    TEST_ASSERT(RecvResync(fdW, got, 5000));
    CSimulation simNew;
    simNew.SeedRng(got.seed);
    MirrorPromoteUnits(simNew);
    for (int t = 0; t <= 0; t++) {
        TurnBatch rb;
        rb.turn = t;
        for (const auto& c : got.commands) {
            if (c.turn == t) rb.commands.push_back(c);
        }
        StepTurn(simNew, rb);
    }
    TEST_ASSERT(simNew.HashState() == simLive.HashState());

    // Turn 1 with both seats live (no resync needed anymore).
    TEST_ASSERT(ClientSend(fdP, 1, {MakeCmd(1, 1, 601, 851986, 1, -1, 400, 0)}));
    TEST_ASSERT(ClientSend(fdW, 1, {MakeCmd(1, 2, 602, 851986, 1, -1, 400, 500)}));
    TurnBatch t1 = host.RunTurn({}, 5000, simLive.HashState());
    TEST_ASSERT(t1.commands.size() == 2);
    TurnBatch s1p, s1w;
    TEST_ASSERT(ClientRecv(fdP, s1p) && ClientRecv(fdW, s1w));
    StepTurn(simLive, t1);
    StepTurn(simNew, t1);
    TEST_ASSERT(simLive.HashState() == simNew.HashState());

    TcpTransport::Close(fdP);
    TcpTransport::Close(fdW);
    TcpTransport::GlobalShutdown();
    return true;
}

bool TestPromoteValidation() {
    TcpTransport::GlobalInit();
    LockstepHost host(2);
    TEST_ASSERT(host.Listen(0));
    const uint16_t port = host.Port();
    TEST_ASSERT(host.PromoteWatcher(-1, 1) == -1);
    TEST_ASSERT(host.PromoteWatcher(0, 1) == -1); // no watchers

    int fdW = TcpTransport::Connect("127.0.0.1", port, 5000);
    TEST_ASSERT(fdW != TcpTransport::kInvalidFd);
    TEST_ASSERT(host.AcceptSpectator(5000) == 0);
    TEST_ASSERT(host.PromoteWatcher(0, 0) == -1); // never seat 0
    TEST_ASSERT(host.PromoteWatcher(0, 9) == -1); // out of range
    TEST_ASSERT(host.PromoteWatcher(5, 1) == -1); // bad watcher
    TEST_ASSERT(host.WatcherCount() == 1); // failed promotes keep it

    int fdP = TcpTransport::Connect("127.0.0.1", port, 5000);
    TEST_ASSERT(fdP != TcpTransport::kInvalidFd);
    TEST_ASSERT(host.AcceptPeer(5000) == 1);
    TEST_ASSERT(host.PromoteWatcher(0, 1) == -1); // occupied
    TEST_ASSERT(host.WatcherCount() == 1);

    // A vanished watcher cannot be promoted (and is reaped).
    TcpTransport::Close(fdW);
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    TEST_ASSERT(host.PromoteWatcher(0, 1) == -1);
    TEST_ASSERT(host.WatcherCount() == 0);
    TcpTransport::Close(fdP);
    TcpTransport::GlobalShutdown();
    return true;
}

bool TestPromoteReclaimsDroppedSeat() {
    TcpTransport::GlobalInit();
    LockstepHost host(2);
    TEST_ASSERT(host.Listen(0));
    const uint16_t port = host.Port();

    int fdP = TcpTransport::Connect("127.0.0.1", port, 5000);
    TEST_ASSERT(fdP != TcpTransport::kInvalidFd);
    TEST_ASSERT(host.AcceptPeer(5000) == 1);
    TEST_ASSERT(ClientSend(fdP, 0, {MakeCmd(0, 1, 102, 851972)}));
    TurnBatch t0 = host.RunTurn({}, 5000, 1u);
    TEST_ASSERT(t0.commands.size() == 1);
    TurnBatch seen;
    TEST_ASSERT(ClientRecv(fdP, seen));
    TcpTransport::Close(fdP); // player vanishes
    TurnBatch t1 = host.RunTurn({}, 500, 2u); // EOF evicts at once
    TEST_ASSERT(t1.commands.empty());
    TEST_ASSERT(host.PeerCount() == 0);

    int fdW = TcpTransport::Connect("127.0.0.1", port, 5000);
    TEST_ASSERT(fdW != TcpTransport::kInvalidFd);
    TEST_ASSERT(host.AcceptSpectator(5000) == 0);
    TEST_ASSERT(host.PromoteWatcher(0, 1) == 1); // reclaim via un-drop
    TEST_ASSERT(host.PeerCount() == 1);
    TEST_ASSERT(ClientSend(fdW, 2, {MakeCmd(2, 1, 102, 851972)}));
    TurnBatch t2 = host.RunTurn({}, 5000, 3u);
    TEST_ASSERT(t2.turn == 2 && t2.commands.size() == 1);
    TEST_ASSERT(t2.commands[0].player == 1);
    TcpTransport::Close(fdW);
    TcpTransport::GlobalShutdown();
    return true;
}
