#include "projects/minimal_rts/minimal_rts_module.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "engine/core/project_manifest.h"

namespace projects::minimal_rts {

MinimalRtsModule::MinimalRtsModule() {
    EnsureRegistered();
    teamCrystals_.fill(0);
}

void MinimalRtsModule::EnsureRegistered() {
    static bool registered = false;
    if (!registered) {
        registered = true;
        auto factory = []() -> std::unique_ptr<engine::world::ISimulationExtension> {
            return std::make_unique<MinimalRtsModule>();
        };
        engine::world::ProjectExtensionRegistry::Instance().RegisterFactory("minimal_rts", factory);
        engine::world::ProjectExtensionRegistry::Instance().RegisterFactory(kModuleId, factory);
    }
}

bool MinimalRtsModule::LoadPrototypesFromJson(std::string_view jsonText) {
    engine::detail::MiniJsonParser parser(jsonText);
    engine::detail::JsonValue root;
    if (!parser.ParseRoot(root)) {
        return false;
    }
    if (root.kind != engine::detail::JsonValue::Kind::Object) {
        return false;
    }
    const auto* protoArray = root.FindField("prototypes");
    if (!protoArray || protoArray->kind != engine::detail::JsonValue::Kind::Array) {
        return false;
    }

    prototypes_.clear();
    for (const auto& item : protoArray->arrayItems) {
        if (item.kind != engine::detail::JsonValue::Kind::Object) {
            continue;
        }
        const std::string idStr = item.GetString("id", "");
        if (idStr.empty()) {
            continue;
        }

        UnitPrototype proto;
        proto.id = engine::TagId(idStr);
        proto.name = idStr;
        const std::string roleStr = item.GetString("role", "combat");
        if (roleStr == "dropoff") proto.role = UnitRole::Dropoff;
        else if (roleStr == "resource") proto.role = UnitRole::ResourceNode;
        else if (roleStr == "harvester") proto.role = UnitRole::Harvester;
        else proto.role = UnitRole::Combat;

        proto.maxHp = std::max<int32_t>(1, item.GetInt("maxHp", 100));
        proto.moveSpeed = static_cast<float>(std::max(0.0, item.GetNumber("moveSpeed", 180.0)));
        proto.attackDamage = std::max<int32_t>(0, item.GetInt("attackDamage", 10));
        proto.attackRange = static_cast<float>(std::max(0.0, item.GetNumber("attackRange", 128.0)));
        proto.carryCapacity = std::max<int32_t>(0, item.GetInt("carryCapacity", 0));

        prototypes_[proto.id] = std::move(proto);
    }
    return !prototypes_.empty();
}

bool MinimalRtsModule::ConfigureFromProjectData(std::string_view sceneOrDataJson) {
    if (!sceneOrDataJson.empty()) {
        if (!LoadPrototypesFromJson(sceneOrDataJson)) {
            return false;
        }
    }
    OnInitializeWorld();
    return true;
}

const UnitPrototype* MinimalRtsModule::FindPrototype(engine::TagId id) const {
    auto it = prototypes_.find(id);
    return (it != prototypes_.end()) ? &it->second : nullptr;
}

EntityId MinimalRtsModule::SpawnEntity(engine::TagId protoId, uint16_t teamId, float x, float y) {
    // Strict team_id bounds check prevents out-of-bounds write into `teamCrystals_`!
    if (teamId >= kMaxTeams) {
        return kNullEntity;
    }
    const UnitPrototype* proto = FindPrototype(protoId);
    if (!proto) {
        return kNullEntity;
    }

    EntityRecord rec;
    rec.id = nextEntityId_++;
    rec.protoId = protoId;
    rec.teamId = teamId;
    rec.role = proto->role;
    rec.x = x;
    rec.y = y;
    rec.hp = proto->maxHp;
    rec.maxHp = proto->maxHp;
    rec.remainingYield = (proto->role == UnitRole::ResourceNode) ? proto->carryCapacity : 0;
    rec.alive = true;

    entities_.push_back(rec);
    return rec.id;
}

const EntityRecord* MinimalRtsModule::GetEntity(EntityId id) const {
    for (const auto& e : entities_) {
        if (e.id == id) return &e;
    }
    return nullptr;
}

EntityRecord* MinimalRtsModule::FindMutableEntity(EntityId id) {
    for (auto& e : entities_) {
        if (e.id == id) return &e;
    }
    return nullptr;
}

size_t MinimalRtsModule::AliveEntityCount() const {
    size_t n = 0;
    for (const auto& e : entities_) {
        if (e.alive) ++n;
    }
    return n;
}

void MinimalRtsModule::QueueCommand(const MinimalRtsCommand& cmd) {
    if (cmd.teamId >= kMaxTeams) {
        return;
    }
    commandQueue_.push_back(cmd);
    std::stable_sort(commandQueue_.begin(), commandQueue_.end(),
                     [](const MinimalRtsCommand& a, const MinimalRtsCommand& b) {
                         if (a.executeTick != b.executeTick) return a.executeTick < b.executeTick;
                         if (a.teamId != b.teamId) return a.teamId < b.teamId;
                         return a.entityId < b.entityId;
                     });
}

void MinimalRtsModule::OnInitializeWorld() {
    entities_.clear();
    commandQueue_.clear();
    teamCrystals_.fill(500);
    nextEntityId_ = 1u;
    tickCount_ = 0u;
    winningTeam_ = -1;

    if (prototypes_.empty()) {
        // Fallback default prototypes if none loaded yet
        static constexpr std::string_view kDefaultJson = R"({
            "prototypes": [
                {"id": "hq_nexus", "role": "dropoff", "maxHp": 1200, "moveSpeed": 0, "attackDamage": 0, "attackRange": 0, "carryCapacity": 0},
                {"id": "crystal_node", "role": "resource", "maxHp": 5000, "moveSpeed": 0, "attackDamage": 0, "attackRange": 0, "carryCapacity": 1500},
                {"id": "drone_harvester", "role": "harvester", "maxHp": 80, "moveSpeed": 180, "attackDamage": 5, "attackRange": 64, "carryCapacity": 25},
                {"id": "pulse_trooper", "role": "combat", "maxHp": 140, "moveSpeed": 220, "attackDamage": 18, "attackRange": 320, "carryCapacity": 0}
            ]
        })";
        LoadPrototypesFromJson(kDefaultJson);
    }

    // Set up a default 2-team skirmish scenario when initialized via generic host
    const EntityId hq0 = SpawnEntity(engine::TagId("hq_nexus"), 0, 128.0f, 128.0f);
    const EntityId node0 = SpawnEntity(engine::TagId("crystal_node"), 2, 260.0f, 128.0f);
    const EntityId drone0 = SpawnEntity(engine::TagId("drone_harvester"), 0, 160.0f, 128.0f);
    const EntityId troop0 = SpawnEntity(engine::TagId("pulse_trooper"), 0, 220.0f, 220.0f);

    const EntityId hq1 = SpawnEntity(engine::TagId("hq_nexus"), 1, 768.0f, 768.0f);
    const EntityId troop1 = SpawnEntity(engine::TagId("pulse_trooper"), 1, 520.0f, 520.0f);
    (void)hq0;
    (void)hq1;

    if (drone0 != kNullEntity && node0 != kNullEntity) {
        QueueCommand({0u, 0u, drone0, EntityRecord::OrderKind::Harvest, 260.0f, 128.0f, node0});
    }
    if (troop0 != kNullEntity) {
        QueueCommand({1u, 0u, troop0, EntityRecord::OrderKind::AttackMove, 520.0f, 520.0f, kNullEntity});
    }
    if (troop1 != kNullEntity) {
        QueueCommand({1u, 1u, troop1, EntityRecord::OrderKind::AttackMove, 220.0f, 220.0f, kNullEntity});
    }
}

EntityId MinimalRtsModule::FindNearestByRole(uint16_t teamFilter,
                                             bool matchTeam,
                                             UnitRole role,
                                             float x,
                                             float y) const {
    EntityId bestId = kNullEntity;
    float bestDistSq = 1e30f;
    for (const auto& e : entities_) {
        if (!e.alive || e.role != role) continue;
        if (matchTeam && e.teamId != teamFilter) continue;
        if (role == UnitRole::ResourceNode && e.remainingYield <= 0) continue;
        const float dx = e.x - x;
        const float dy = e.y - y;
        const float d2 = dx * dx + dy * dy;
        if (d2 < bestDistSq) {
            bestDistSq = d2;
            bestId = e.id;
        }
    }
    return bestId;
}

void MinimalRtsModule::StepSingleTick(float fixedDt) {
    OnTickPhase(engine::world::TickPhase::CommandIngest, fixedDt);
    OnTickPhase(engine::world::TickPhase::OrderBehavior, fixedDt);
    OnTickPhase(engine::world::TickPhase::MovementSteering, fixedDt);
    OnTickPhase(engine::world::TickPhase::CombatProjectiles, fixedDt);
    OnTickPhase(engine::world::TickPhase::PostTickCleanup, fixedDt);
}

void MinimalRtsModule::OnTickPhase(engine::world::TickPhase phase, float fixedDt) {
    switch (phase) {
        case engine::world::TickPhase::CommandIngest: {
            auto it = commandQueue_.begin();
            while (it != commandQueue_.end()) {
                if (it->executeTick <= tickCount_) {
                    if (EntityRecord* ent = FindMutableEntity(it->entityId)) {
                        if (ent->alive && ent->teamId == it->teamId) {
                            ent->order = it->order;
                            ent->targetX = it->targetX;
                            ent->targetY = it->targetY;
                            ent->targetEntity = it->targetEntity;
                        }
                    }
                    it = commandQueue_.erase(it);
                } else {
                    ++it;
                }
            }
            break;
        }

        case engine::world::TickPhase::OrderBehavior: {
            for (auto& e : entities_) {
                if (!e.alive) continue;
                const UnitPrototype* proto = FindPrototype(e.protoId);
                const int32_t maxCarry = proto ? proto->carryCapacity : 25;

                if (e.order == EntityRecord::OrderKind::Harvest) {
                    const EntityRecord* node = GetEntity(e.targetEntity);
                    if (!node || !node->alive || node->remainingYield <= 0) {
                        e.targetEntity = FindNearestByRole(e.teamId, false, UnitRole::ResourceNode, e.x, e.y);
                        node = GetEntity(e.targetEntity);
                    }
                    if (!node) {
                        e.order = EntityRecord::OrderKind::Idle;
                        continue;
                    }
                    e.targetX = node->x;
                    e.targetY = node->y;
                    const float dx = node->x - e.x;
                    const float dy = node->y - e.y;
                    if (dx * dx + dy * dy <= 48.0f * 48.0f) {
                        EntityRecord* mutNode = FindMutableEntity(node->id);
                        const int32_t take = std::min<int32_t>(maxCarry - e.carriedCrystals, mutNode->remainingYield);
                        e.carriedCrystals += take;
                        mutNode->remainingYield -= take;
                        if (e.carriedCrystals >= maxCarry || mutNode->remainingYield <= 0) {
                            const EntityId dropId = FindNearestByRole(e.teamId, true, UnitRole::Dropoff, e.x, e.y);
                            if (dropId != kNullEntity) {
                                e.order = EntityRecord::OrderKind::ReturnCargo;
                                e.targetEntity = dropId;
                            }
                        }
                    }
                } else if (e.order == EntityRecord::OrderKind::ReturnCargo) {
                    const EntityRecord* drop = GetEntity(e.targetEntity);
                    if (!drop || !drop->alive) {
                        e.order = EntityRecord::OrderKind::Idle;
                        continue;
                    }
                    e.targetX = drop->x;
                    e.targetY = drop->y;
                    const float dx = drop->x - e.x;
                    const float dy = drop->y - e.y;
                    if (dx * dx + dy * dy <= 56.0f * 56.0f) {
                        if (e.teamId < kMaxTeams && e.carriedCrystals > 0) {
                            teamCrystals_[e.teamId] += e.carriedCrystals;
                            e.carriedCrystals = 0;
                        }
                        const EntityId nextNode = FindNearestByRole(e.teamId, false, UnitRole::ResourceNode, e.x, e.y);
                        if (nextNode != kNullEntity) {
                            e.order = EntityRecord::OrderKind::Harvest;
                            e.targetEntity = nextNode;
                        } else {
                            e.order = EntityRecord::OrderKind::Idle;
                        }
                    }
                }
            }
            break;
        }

        case engine::world::TickPhase::MovementSteering: {
            for (auto& e : entities_) {
                if (!e.alive || e.order == EntityRecord::OrderKind::Idle) continue;
                const UnitPrototype* proto = FindPrototype(e.protoId);
                const float speed = proto ? proto->moveSpeed : 0.0f;
                if (speed <= 0.0f) continue;

                // Stop moving if combat unit has an enemy in range during AttackMove
                if (e.order == EntityRecord::OrderKind::AttackMove && proto && proto->attackDamage > 0) {
                    bool enemyInRange = false;
                    const float r2 = proto->attackRange * proto->attackRange;
                    for (const auto& other : entities_) {
                        if (!other.alive || other.teamId == e.teamId || other.role == UnitRole::ResourceNode) continue;
                        const float dx = other.x - e.x;
                        const float dy = other.y - e.y;
                        if (dx * dx + dy * dy <= r2) {
                            enemyInRange = true;
                            break;
                        }
                    }
                    if (enemyInRange) continue;
                }

                const float dx = e.targetX - e.x;
                const float dy = e.targetY - e.y;
                const float dist = std::sqrt(dx * dx + dy * dy);
                const float step = speed * fixedDt;
                if (dist <= step || dist <= 1e-4f) {
                    e.x = e.targetX;
                    e.y = e.targetY;
                    if (e.order == EntityRecord::OrderKind::Move) {
                        e.order = EntityRecord::OrderKind::Idle;
                    }
                } else {
                    e.x += (dx / dist) * step;
                    e.y += (dy / dist) * step;
                }
            }
            break;
        }

        case engine::world::TickPhase::CombatProjectiles: {
            for (auto& e : entities_) {
                if (!e.alive) continue;
                if (e.cooldownSec > 0.0f) {
                    e.cooldownSec = std::max(0.0f, e.cooldownSec - fixedDt);
                }
                const UnitPrototype* proto = FindPrototype(e.protoId);
                if (!proto || proto->attackDamage <= 0 || e.cooldownSec > 0.0f) continue;

                const float r2 = proto->attackRange * proto->attackRange;
                EntityRecord* bestTarget = nullptr;
                float bestDistSq = r2;
                for (auto& cand : entities_) {
                    if (!cand.alive || cand.teamId == e.teamId || cand.role == UnitRole::ResourceNode) continue;
                    const float dx = cand.x - e.x;
                    const float dy = cand.y - e.y;
                    const float d2 = dx * dx + dy * dy;
                    if (d2 <= bestDistSq) {
                        bestDistSq = d2;
                        bestTarget = &cand;
                    }
                }
                if (bestTarget) {
                    bestTarget->hp -= proto->attackDamage;
                    e.cooldownSec = 0.75f;
                    if (bestTarget->hp <= 0) {
                        bestTarget->hp = 0;
                        bestTarget->alive = false;
                    }
                }
            }
            break;
        }

        case engine::world::TickPhase::PostTickCleanup: {
            ++tickCount_;
            if (winningTeam_ < 0) {
                for (uint16_t t = 0; t < kMaxTeams; ++t) {
                    if (teamCrystals_[t] >= 1000) {
                        winningTeam_ = static_cast<int>(t);
                        break;
                    }
                }
            }
            break;
        }
    }
}

uint64_t MinimalRtsModule::ComputeDeterministicHash() const {
    uint64_t h = 14695981039346656037ULL;
    auto mixU64 = [&](uint64_t v) {
        for (int b = 0; b < 8; ++b) {
            h ^= (v >> (b * 8)) & 0xFFULL;
            h *= 1099511628211ULL;
        }
    };
    auto mixF32 = [&](float f) {
        // Quantize to 1/1024th unit to avoid -0.0f representation drift
        const int64_t q = static_cast<int64_t>(std::llround(static_cast<double>(f) * 1024.0));
        mixU64(static_cast<uint64_t>(q));
    };

    mixU64(tickCount_);
    for (uint16_t t = 0; t < kMaxTeams; ++t) {
        mixU64(static_cast<uint64_t>(static_cast<uint32_t>(teamCrystals_[t])));
    }
    mixU64(static_cast<uint64_t>(entities_.size()));
    for (const auto& e : entities_) {
        mixU64(e.id);
        mixU64(e.protoId.Value());
        mixU64(e.teamId);
        mixU64(e.alive ? 1u : 0u);
        mixU64(static_cast<uint64_t>(static_cast<uint32_t>(e.hp)));
        mixU64(static_cast<uint64_t>(static_cast<uint32_t>(e.carriedCrystals)));
        mixU64(static_cast<uint64_t>(static_cast<uint32_t>(e.remainingYield)));
        mixF32(e.x);
        mixF32(e.y);
    }
    return h;
}

void MinimalRtsModule::WriteChunk(engine::world::BinaryWriter& writer) const {
    writer.WriteU64(tickCount_);
    writer.WriteU32(nextEntityId_);
    writer.WriteI32(winningTeam_);
    for (uint16_t t = 0; t < kMaxTeams; ++t) {
        writer.WriteI32(teamCrystals_[t]);
    }
    writer.WriteU32(static_cast<uint32_t>(entities_.size()));
    for (const auto& e : entities_) {
        writer.WriteU32(e.id);
        writer.WriteU32(e.protoId.Value());
        writer.WriteU16(e.teamId);
        writer.WriteU8(static_cast<uint8_t>(e.role));
        writer.WriteF32(e.x);
        writer.WriteF32(e.y);
        writer.WriteI32(e.hp);
        writer.WriteI32(e.maxHp);
        writer.WriteI32(e.carriedCrystals);
        writer.WriteI32(e.remainingYield);
        writer.WriteU8(e.alive ? 1u : 0u);
        writer.WriteU8(static_cast<uint8_t>(e.order));
        writer.WriteF32(e.targetX);
        writer.WriteF32(e.targetY);
        writer.WriteU32(e.targetEntity);
        writer.WriteF32(e.cooldownSec);
    }
}

bool MinimalRtsModule::ReadChunk(engine::world::BinaryReader& reader, uint32_t version) {
    if (version != 1u) return false;
    uint64_t tick = 0;
    uint32_t nextId = 1;
    int32_t winTeam = -1;
    if (!reader.ReadU64(tick) || !reader.ReadU32(nextId) || !reader.ReadI32(winTeam)) {
        return false;
    }
    std::array<int32_t, kMaxTeams> crystals{};
    for (uint16_t t = 0; t < kMaxTeams; ++t) {
        if (!reader.ReadI32(crystals[t])) return false;
    }
    uint32_t entCount = 0;
    if (!reader.ReadU32(entCount) || entCount > 100000u) {
        return false;
    }

    std::vector<EntityRecord> loaded;
    loaded.reserve(entCount);
    for (uint32_t i = 0; i < entCount; ++i) {
        EntityRecord e;
        uint32_t protoRaw = 0;
        uint8_t roleByte = 0;
        uint8_t aliveByte = 0;
        uint8_t orderByte = 0;
        if (!reader.ReadU32(e.id) ||
            !reader.ReadU32(protoRaw) ||
            !reader.ReadU16(e.teamId) ||
            !reader.ReadU8(roleByte) ||
            !reader.ReadF32(e.x) ||
            !reader.ReadF32(e.y) ||
            !reader.ReadI32(e.hp) ||
            !reader.ReadI32(e.maxHp) ||
            !reader.ReadI32(e.carriedCrystals) ||
            !reader.ReadI32(e.remainingYield) ||
            !reader.ReadU8(aliveByte) ||
            !reader.ReadU8(orderByte) ||
            !reader.ReadF32(e.targetX) ||
            !reader.ReadF32(e.targetY) ||
            !reader.ReadU32(e.targetEntity) ||
            !reader.ReadF32(e.cooldownSec)) {
            return false;
        }
        if (e.teamId >= kMaxTeams) {
            return false;
        }
        e.protoId = engine::TagId(protoRaw);
        e.role = static_cast<UnitRole>(roleByte);
        e.alive = (aliveByte != 0u);
        e.order = static_cast<EntityRecord::OrderKind>(orderByte);
        loaded.push_back(e);
    }

    tickCount_ = tick;
    nextEntityId_ = nextId;
    winningTeam_ = winTeam;
    teamCrystals_ = crystals;
    entities_ = std::move(loaded);
    commandQueue_.clear();
    return true;
}

namespace {
struct MinimalRtsAutoRegistrar {
    MinimalRtsAutoRegistrar() { MinimalRtsModule::EnsureRegistered(); }
} g_minimalRtsAutoRegistrar;
} // namespace

} // namespace projects::minimal_rts

#ifdef MINIMAL_RTS_SHARED_PLUGIN
#if defined(_WIN32)
#define ENGINE_EXT_PLUGIN_EXPORT extern "C" __declspec(dllexport)
#else
#define ENGINE_EXT_PLUGIN_EXPORT extern "C" __attribute__((visibility("default")))
#endif

ENGINE_EXT_PLUGIN_EXPORT const char* EngineGetExtensionPluginId() {
    return projects::minimal_rts::MinimalRtsModule::kModuleId;
}

ENGINE_EXT_PLUGIN_EXPORT engine::world::ISimulationExtension* EngineCreateSimulationExtension() {
    return new projects::minimal_rts::MinimalRtsModule();
}

ENGINE_EXT_PLUGIN_EXPORT void EngineDestroySimulationExtension(engine::world::ISimulationExtension* ext) {
    delete ext;
}
#endif // MINIMAL_RTS_SHARED_PLUGIN
