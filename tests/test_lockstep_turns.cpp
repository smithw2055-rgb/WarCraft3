// Lockstep turn tests: batch wire format, order reconstruction, host
// collection/sorting, and a 2-peer loopback game (same seed, same batch
// -> identical hashes; tampered peer -> desync detected).
#include <cmath>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>

#include "data/combat_types.h"
#include "ecs/abilities/ability_move.h"
#include "ecs/csimulation.h"
#include "ecs/lockstep.h"
#include "ecs/orders/order.h"
#include "ecs/player.h"
#include "ecs/unit.h"
#include "ecs/unit_type.h"
#include "net/tcp_transport.h"
#include "net/turn_batch.h"
#include "net/turn_manager.h"

#define TEST_ASSERT(cond) do { if (!(cond)) { std::cerr << "ASSERT @" << __LINE__ << ": " #cond "\n"; return false; } } while (0)

namespace {

SimCommand MakeCmd(int turn, int player, int unit, int order, int kind = 0,
                   int target = -1, float x = 0, float y = 0) {
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

CUnitType* MakeTurnType() {
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

void MirrorUnits(CSimulation& sim) {
    // AdoptUnit skips the default ability set, so attach move abilities
    // explicitly (mirrors CSimulation::CreateUnit).
    auto* a = sim.AdoptUnit(std::make_unique<CUnit>(101, MakeTurnType(), 0, War3ID("hfoo"), 0, 0, 0));
    auto* b = sim.AdoptUnit(std::make_unique<CUnit>(102, MakeTurnType(), 1, War3ID("hfoo"), 0, 500, 0));
    a->AddAbility(sim, std::make_unique<CAbilityMove>(1001));
    b->AddAbility(sim, std::make_unique<CAbilityMove>(1002));
    sim.Update();
}

} // namespace

bool TestTurnBatchWireRoundTrip() {
    TurnBatch batch;
    batch.turn = 41;
    batch.commands.push_back(MakeCmd(41, 0, 101, 851986, 1, -1, 5.0f, 6.0f));
    batch.commands.push_back(MakeCmd(41, 1, 102, 851972));
    batch.stateHash = 0x123456789ABCDEF0ull;
    auto wire = batch.Serialize();
    TurnBatch back;
    TEST_ASSERT(TurnBatch::Deserialize(wire.data(), wire.size(), back));
    TEST_ASSERT(back.turn == 41);
    TEST_ASSERT(back.commands.size() == 2);
    TEST_ASSERT(back.commands[0].unit == 101 && back.commands[0].x == 5.0f);
    TEST_ASSERT(back.commands[1].orderId == 851972);
    TEST_ASSERT(back.stateHash == 0x123456789ABCDEF0ull);
    // Empty batch + truncation rejection.
    TurnBatch empty;
    auto wire2 = empty.Serialize();
    TurnBatch back2;
    TEST_ASSERT(TurnBatch::Deserialize(wire2.data(), wire2.size(), back2));
    TEST_ASSERT(back2.commands.empty());
    TEST_ASSERT(!TurnBatch::Deserialize(wire.data(), wire.size() - 1, back2));
    TEST_ASSERT(!TurnBatch::Deserialize(wire.data(), 3, back2));
    return true;
}

bool TestOrderFromCommandShapes() {
    CSimulation sim;
    MirrorUnits(sim);
    // Point order.
    auto pt = OrderFromCommand(sim, MakeCmd(0, 0, 101, 851986, 1, -1, 7.0f, 8.0f));
    TEST_ASSERT(pt && pt->GetTargetKind() == COrder::TargetKind::POINT);
    // No-target order.
    auto nt = OrderFromCommand(sim, MakeCmd(0, 0, 101, 851972));
    TEST_ASSERT(nt && nt->GetTargetKind() == COrder::TargetKind::NONE);
    // Widget order onto a live unit.
    auto wg = OrderFromCommand(sim, MakeCmd(0, 0, 101, 851983, 2, 102));
    TEST_ASSERT(wg && wg->GetTargetKind() == COrder::TargetKind::WIDGET);
    // Stale handle -> null (skipped deterministically by ApplyBatch).
    auto stale = OrderFromCommand(sim, MakeCmd(0, 0, 101, 851983, 2, 9999));
    TEST_ASSERT(!stale);
    // ApplyBatch counts only live reconstructions.
    TurnBatch batch;
    batch.commands.push_back(MakeCmd(0, 0, 101, 851972));
    batch.commands.push_back(MakeCmd(0, 0, 101, 851983, 2, 9999));
    TEST_ASSERT(ApplyBatch(sim, batch) == 1);
    return true;
}

bool TestTurnManagerCollectsAndSorts() {
    TurnManager mgr(2);
    TEST_ASSERT(!mgr.IsComplete());
    TEST_ASSERT(!mgr.SubmitPlayerCommands(7, {})); // bad player
    TEST_ASSERT(mgr.SubmitPlayerCommands(1, {MakeCmd(0, 1, 102, 851972)}));
    TEST_ASSERT(mgr.HasSubmitted(1));
    TEST_ASSERT(!mgr.IsComplete());
    TEST_ASSERT(!mgr.SubmitPlayerCommands(1, {})); // double submit
    TEST_ASSERT(mgr.SubmitPlayerCommands(0, {MakeCmd(0, 0, 101, 851986, 1, -1, 1, 2),
                                            MakeCmd(0, 0, 101, 851972)}));
    TEST_ASSERT(mgr.IsComplete());
    TurnBatch batch = mgr.TakeBatch();
    TEST_ASSERT(batch.turn == 0);
    TEST_ASSERT(batch.commands.size() == 3);
    // Deterministic order: player 0 first, then by unit/order.
    TEST_ASSERT(batch.commands[0].player == 0);
    TEST_ASSERT(batch.commands[2].player == 1);
    TEST_ASSERT(mgr.Turn() == 1);
    TEST_ASSERT(!mgr.IsComplete()); // fresh turn, nothing submitted
    return true;
}

bool TestLoopbackLockstepGameStaysInSync() {
    TcpTransport::GlobalInit();
    CSimulation host, client;
    host.SeedRng(9001);
    client.SeedRng(9001);
    MirrorUnits(host);
    MirrorUnits(client);

    const int listenFd = TcpTransport::Listen(0);
    TEST_ASSERT(listenFd != TcpTransport::kInvalidFd);
    const uint16_t port = TcpTransport::BoundPort(listenFd);

    // Client thread: send turn-0 commands, receive the merged batch.
    TurnBatch clientBatch;
    bool clientGot = false;
    std::thread peer([&] {
        const int fd = TcpTransport::Connect("127.0.0.1", port, 5000);
        if (fd == TcpTransport::kInvalidFd) return;
        TurnBatch my;
        my.turn = 0;
        my.commands.push_back(MakeCmd(0, 1, 102, 851986, 1, -1, 300, 500));
        auto wire = my.Serialize();
        if (!TcpTransport::SendMessage(fd, wire.data(), wire.size())) {
            TcpTransport::Close(fd);
            return;
        }
        std::vector<uint8_t> reply;
        if (TcpTransport::RecvMessage(fd, reply, 5000)) {
            clientGot = TurnBatch::Deserialize(reply.data(), reply.size(), clientBatch);
        }
        TcpTransport::Close(fd);
    });

    const int hostFd = TcpTransport::Accept(listenFd, 5000);
    TEST_ASSERT(hostFd != TcpTransport::kInvalidFd);
    // Host receives the client's turn-0 list.
    std::vector<uint8_t> incoming;
    TEST_ASSERT(TcpTransport::RecvMessage(hostFd, incoming, 5000));
    TurnBatch clientTurn;
    TEST_ASSERT(TurnBatch::Deserialize(incoming.data(), incoming.size(), clientTurn));

    // Host merges both seats (host moves unit 101 east) and broadcasts.
    TurnManager mgr(2);
    TEST_ASSERT(mgr.SubmitPlayerCommands(0, {MakeCmd(0, 0, 101, 851986, 1, -1, 300, 0)}));
    TEST_ASSERT(mgr.SubmitPlayerCommands(1, clientTurn.commands));
    TurnBatch batch = mgr.TakeBatch();
    auto wire = batch.Serialize();
    TEST_ASSERT(TcpTransport::SendMessage(hostFd, wire.data(), wire.size()));
    peer.join();
    TEST_ASSERT(clientGot);
    TEST_ASSERT(clientBatch.commands.size() == 2);

    // Both peers apply the same batch and step: identical hashes.
    TEST_ASSERT(ApplyBatch(host, batch) == 2);
    TEST_ASSERT(ApplyBatch(client, clientBatch) == 2);
    for (int i = 0; i < 40; i++) {
        host.Update();
        client.Update();
    }
    TEST_ASSERT(host.GetUnit(101) != nullptr && client.GetUnit(101) != nullptr);
    const float hx = host.GetUnit(101)->GetX();
    TEST_ASSERT(hx > 1.0f); // units actually marched (non-vacuous)
    TEST_ASSERT(client.GetUnit(101)->GetX() == hx);
    TEST_ASSERT(host.HashState() == client.HashState());

    TcpTransport::Close(hostFd);
    TcpTransport::Close(listenFd);
    TcpTransport::GlobalShutdown();
    return true;
}

bool TestDesyncIsDetected() {
    CSimulation host, client;
    host.SeedRng(31337);
    client.SeedRng(31338); // tampered seed: combat/RNG streams diverge
    MirrorUnits(host);
    MirrorUnits(client);
    // Same attack batch on both: dice rolls diverge with different seeds.
    TurnBatch batch;
    batch.turn = 0;
    auto* hu = host.GetUnit(102);
    auto* cu = client.GetUnit(102);
    hu->SetEvasionChance(0.5f);
    cu->SetEvasionChance(0.5f);
    for (int i = 0; i < 8; i++) {
        host.GetCombatSystem()->CalculateDamage(host.GetUnit(101), hu, true, false,
                                               CAttackType::NORMAL, CDamageType::NORMAL,
                                               10, 0, 2, 6);
        client.GetCombatSystem()->CalculateDamage(client.GetUnit(101), cu, true, false,
                                                 CAttackType::NORMAL, CDamageType::NORMAL,
                                                 10, 0, 2, 6);
        hu->SetLife(host, 10000.0f);
        cu->SetLife(client, 10000.0f);
    }
    // Hashes carry the RNG states: divergence is observable.
    TEST_ASSERT(host.HashState() != client.HashState());
    // And the batch hash field is how peers notice: host stamps, client differs.
    batch.stateHash = host.HashState();
    TEST_ASSERT(batch.stateHash != client.HashState());
    return true;
}
