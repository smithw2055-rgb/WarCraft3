#include "scripting/jass_natives_internal.h"
void JassNatives::RegisterGroupNatives() {
    vm_.RegisterNative("CreateGroup", [data = data_](const std::vector<JassValue>&) -> JassValue { auto group = std::make_shared<JassGroup>(); int id = data->nextGroupId++; data->groups[id] = group; return JassValue::Int(data->AllocHandle(JassHandleInfo::GROUP, id)); });
    vm_.RegisterNative("DestroyGroup", [data = data_](const std::vector<JassValue>& args) -> JassValue { if (args.empty()) return JassValue::Null(); int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::GROUP); if (simId >= 0) { data->groups.erase(simId); data->FreeHandle(args[0].intVal); } return JassValue::Null(); });
    vm_.RegisterNative("GroupAddUnit", [data = data_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2) return JassValue::Null(); int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::GROUP); if (simId >= 0) { auto it = data->groups.find(simId); if (it != data->groups.end()) { auto& vec = it->second->unitHandles; if (std::find(vec.begin(), vec.end(), args[1].intVal) == vec.end()) vec.push_back(args[1].intVal); } } return JassValue::Null(); });
    vm_.RegisterNative("GroupRemoveUnit", [data = data_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2) return JassValue::Null(); int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::GROUP); if (simId >= 0) { auto it = data->groups.find(simId); if (it != data->groups.end()) { auto& vec = it->second->unitHandles; vec.erase(std::remove(vec.begin(), vec.end(), args[1].intVal), vec.end()); } } return JassValue::Null(); });
    vm_.RegisterNative("GroupClear", [data = data_](const std::vector<JassValue>& args) -> JassValue { if (args.empty()) return JassValue::Null(); int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::GROUP); if (simId >= 0) { auto it = data->groups.find(simId); if (it != data->groups.end()) it->second->unitHandles.clear(); } return JassValue::Null(); });
    auto filterFuncAt = [](const std::vector<JassValue>& args, size_t index) -> std::string { return (args.size() > index && args[index].type == JassValue::STRING) ? args[index].strVal : ""; };
    auto acceptFilteredUnit = [data = data_, vmPtr = vmPtr_](int unitHandle, const std::string& filterFunc) { if (filterFunc.empty() || !vmPtr) return true; const int previousFilterUnit = data->filterUnitHandle; data->filterUnitHandle = unitHandle; const bool accepted = JassValueToBool(CallSyncFunction(vmPtr, filterFunc)); data->filterUnitHandle = previousFilterUnit; return accepted; };
    vm_.RegisterNative("GroupEnumUnitsOfType", [data = data_, sim = sim_, filterFuncAt, acceptFilteredUnit](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::GROUP);
        if (simId < 0) return JassValue::Null();
        auto git = data->groups.find(simId);
        if (git == data->groups.end()) return JassValue::Null();
        const std::string unitName = args[1].type == JassValue::STRING
            ? args[1].strVal
            : "";
        const War3ID rawcodeType((uint32_t)args[1].intVal);
        const std::string filterFunc = filterFuncAt(args, 2);
        git->second->unitHandles.clear();
        if (sim) {
            for (auto* unit : sim->GetUnits()) {
                const CUnitType* unitType = unit ? unit->GetUnitType() : nullptr;
                const bool typeMatches = unitType
                    ? (!unitName.empty() ? unitType->GetLegacyName() == unitName
                                         : unit->GetTypeId() == rawcodeType)
                    : false;
                const int unitHandle = data->AllocHandle(JassHandleInfo::UNIT, unit->GetHandleId());
                if (typeMatches && acceptFilteredUnit(unitHandle, filterFunc))
                    git->second->unitHandles.push_back(unitHandle);
            }
        }
        data->filterUnitHandle = -1;
        return JassValue::Null();
    });
    vm_.RegisterNative("GroupEnumUnitsInRange", [data = data_, sim = sim_, filterFuncAt, acceptFilteredUnit](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 4) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::GROUP);
        if (simId < 0) return JassValue::Null();
        auto git = data->groups.find(simId);
        if (git == data->groups.end()) return JassValue::Null();
        float x = args[1].AsReal(), y = args[2].AsReal(), radius = args[3].AsReal();
        float rSq = radius * radius;
        const std::string filterFunc = filterFuncAt(args, 4);
        git->second->unitHandles.clear();
        if (sim) {
            for (auto* unit : sim->GetUnits()) {
                float dx = unit->GetX() - x, dy = unit->GetY() - y;
                const int unitHandle = data->AllocHandle(JassHandleInfo::UNIT, unit->GetHandleId());
                if (dx * dx + dy * dy <= rSq && acceptFilteredUnit(unitHandle, filterFunc))
                    git->second->unitHandles.push_back(unitHandle);
            }
        }
        data->filterUnitHandle = -1;
        return JassValue::Null();
    });
    vm_.RegisterNative("GroupEnumUnitsOfPlayer", [data = data_, sim = sim_, filterFuncAt, acceptFilteredUnit](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::GROUP);
        if (simId < 0) return JassValue::Null();
        auto git = data->groups.find(simId);
        if (git == data->groups.end()) return JassValue::Null();
        int playerSimId = data->GetSimHandle(args[1].intVal, JassHandleInfo::PLAYER);
        const std::string filterFunc = filterFuncAt(args, 2);
        git->second->unitHandles.clear();
        if (sim) {
            for (auto* unit : sim->GetUnits()) {
                const int unitHandle = data->AllocHandle(JassHandleInfo::UNIT, unit->GetHandleId());
                if (unit->GetPlayerIndex() == playerSimId && acceptFilteredUnit(unitHandle, filterFunc))
                    git->second->unitHandles.push_back(unitHandle);
            }
        }
        data->filterUnitHandle = -1;
        return JassValue::Null();
    });
    vm_.RegisterNative("GroupEnumUnitsInRect", [data = data_, sim = sim_, filterFuncAt, acceptFilteredUnit](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        int gid = data->GetSimHandle(args[0].intVal, JassHandleInfo::GROUP);
        if (gid < 0) return JassValue::Null();
        auto git = data->groups.find(gid);
        if (git == data->groups.end()) return JassValue::Null();
        int rid = data->GetSimHandle(args[1].intVal, JassHandleInfo::RECT);
        if (rid < 0) return JassValue::Null();
        auto rit = data->rects.find(rid);
        if (rit == data->rects.end()) return JassValue::Null();
        float minX = rit->second->minX, minY = rit->second->minY;
        float maxX = rit->second->maxX, maxY = rit->second->maxY;
        const std::string filterFunc = filterFuncAt(args, 2);
        git->second->unitHandles.clear();
        if (sim) {
            for (auto* unit : sim->GetUnits()) {
                float ux = unit->GetX(), uy = unit->GetY();
                const int unitHandle = data->AllocHandle(JassHandleInfo::UNIT, unit->GetHandleId());
                if (ux >= minX && ux <= maxX && uy >= minY && uy <= maxY && acceptFilteredUnit(unitHandle, filterFunc))
                    git->second->unitHandles.push_back(unitHandle);
            }
        }
        data->filterUnitHandle = -1;
        return JassValue::Null();
    });
    vm_.RegisterNative("GroupEnumUnitsSelected", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty() || !sim) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::GROUP);
        if (simId < 0) return JassValue::Null();
        auto git = data->groups.find(simId);
        if (git == data->groups.end()) return JassValue::Null();
        git->second->unitHandles.clear();
        for (int unitSimId : data->selectedUnits) {
            if (sim->GetUnit(unitSimId)) {
                git->second->unitHandles.push_back(
                    data->AllocHandle(JassHandleInfo::UNIT, unitSimId));
            }
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("GroupImmediateOrder", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Bool(false);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::GROUP);
        if (simId < 0) return JassValue::Bool(false);
        auto git = data->groups.find(simId);
        if (git == data->groups.end()) return JassValue::Bool(false);
        int orderId = ParseOrderString(args[1].strVal);
        if (orderId <= 0) return JassValue::Bool(false);
        for (int uh : git->second->unitHandles) {
            int uSim = data->GetSimHandle(uh, JassHandleInfo::UNIT);
            if (uSim >= 0 && sim) {
                CUnit* u = sim->GetUnit(uSim);
                if (u) u->IssueOrder(std::make_unique<COrderNoTarget>(orderId));
            }
        }
        return JassValue::Bool(true);
    });
    vm_.RegisterNative("GroupPointOrder", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 4) return JassValue::Bool(false);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::GROUP);
        if (simId < 0) return JassValue::Bool(false);
        auto git = data->groups.find(simId);
        if (git == data->groups.end()) return JassValue::Bool(false);
        int orderId = ParseOrderString(args[1].strVal);
        if (orderId <= 0) return JassValue::Bool(false);
        float x = args[2].realVal, y = args[3].realVal;
        for (int uh : git->second->unitHandles) {
            int uSim = data->GetSimHandle(uh, JassHandleInfo::UNIT);
            if (uSim >= 0 && sim) {
                CUnit* u = sim->GetUnit(uSim);
                if (u) u->IssueOrder(std::make_unique<COrderTargetPoint>(orderId, x, y));
            }
        }
        return JassValue::Bool(true);
    });
    vm_.RegisterNative("GroupTargetOrder", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3) return JassValue::Bool(false);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::GROUP);
        if (simId < 0) return JassValue::Bool(false);
        auto git = data->groups.find(simId);
        if (git == data->groups.end()) return JassValue::Bool(false);
        int orderId = ParseOrderString(args[1].strVal);
        if (orderId <= 0) return JassValue::Bool(false);
        CWidget* target = ResolveOrderTargetWidget(data, sim, args[2].intVal);
        if (!target || !sim) return JassValue::Bool(false);
        for (int uh : git->second->unitHandles) {
            int uSim = data->GetSimHandle(uh, JassHandleInfo::UNIT);
            if (uSim >= 0) {
                CUnit* u = sim->GetUnit(uSim);
                if (u) u->IssueOrder(std::make_unique<COrderTargetWidget>(orderId, target));
            }
        }
        return JassValue::Bool(true);
    });
    vm_.RegisterNative("ForGroup", [data = data_, vmPtr = vmPtr_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::GROUP);
        if (simId < 0) return JassValue::Null();
        auto git = data->groups.find(simId);
        if (git == data->groups.end()) return JassValue::Null();
        const std::string& funcName = args[1].strVal;
        if (funcName.empty()) return JassValue::Null();
        const int previousEnumUnit = data->enumUnitHandle;
        const std::vector<int> snapshot = git->second->unitHandles;
        for (int uh : snapshot) {
            data->enumUnitHandle = uh;
            ExecuteSyncFunction(vmPtr, funcName);
        }
        data->enumUnitHandle = previousEnumUnit;
        return JassValue::Null();
    });
    vm_.RegisterNative("FirstOfGroup", [data = data_](const std::vector<JassValue>& args) -> JassValue { if (args.empty()) return JassValue::Int(0); int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::GROUP); if (simId < 0) return JassValue::Int(0); auto git = data->groups.find(simId); if (git == data->groups.end() || git->second->unitHandles.empty()) return JassValue::Int(0); return JassValue::Int(git->second->unitHandles.front()); });
    vm_.RegisterNative("CountUnitsInGroup", [data = data_](const std::vector<JassValue>& args) -> JassValue { if (args.empty()) return JassValue::Int(0); int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::GROUP); if (simId < 0) return JassValue::Int(0); auto git = data->groups.find(simId); if (git == data->groups.end()) return JassValue::Int(0); return JassValue::Int((int)git->second->unitHandles.size()); });
    vm_.RegisterNative("IsUnitInGroup", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Bool(false);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::GROUP);
        if (simId < 0) return JassValue::Bool(false);
        auto git = data->groups.find(simId);
        if (git == data->groups.end()) return JassValue::Bool(false);
        auto& vec = git->second->unitHandles;
        return JassValue::Bool(std::find(vec.begin(), vec.end(), args[1].intVal) != vec.end());
    });
    auto groupSize = [data = data_](const std::vector<JassValue>& args) -> JassValue { if (args.empty()) return JassValue::Int(0); int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::GROUP); if (simId < 0) return JassValue::Int(0); auto git = data->groups.find(simId); if (git == data->groups.end()) return JassValue::Int(0); return JassValue::Int((int)git->second->unitHandles.size()); };
    vm_.RegisterNative("GroupGetSize", groupSize);
    vm_.RegisterNative("BlzGroupGetSize", groupSize);
    auto groupUnitAt = [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Int(0);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::GROUP);
        if (simId < 0) return JassValue::Int(0);
        auto git = data->groups.find(simId);
        if (git == data->groups.end()) return JassValue::Int(0);
        const int index = args[1].intVal;
        if (index < 0 || index >= (int)git->second->unitHandles.size()) return JassValue::Int(0);
        return JassValue::Int(git->second->unitHandles[(size_t)index]);
    };
    vm_.RegisterNative("GroupUnitAt", groupUnitAt);
    vm_.RegisterNative("BlzGroupUnitAt", groupUnitAt);
    auto enumInRangeOfUnit = [data = data_, sim = sim_, vmPtr = vmPtr_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3) return JassValue::Null();
        const int groupSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::GROUP);
        if (groupSim < 0) return JassValue::Null();
        auto git = data->groups.find(groupSim);
        if (git == data->groups.end()) return JassValue::Null();
        const int centerSim = data->GetSimHandle(args[1].intVal, JassHandleInfo::UNIT);
        CUnit* center = (centerSim >= 0 && sim) ? sim->GetUnit(centerSim) : nullptr;
        if (!center) return JassValue::Null();
        const float radius = args[2].type == JassValue::REAL ? args[2].realVal : (float)args[2].intVal;
        const std::string filterFunc = (args.size() > 3 && args[3].type == JassValue::STRING) ? args[3].strVal : "";
        int countLimit = -1;
        if (args.size() > 4 && args[4].type == JassValue::INTEGER) countLimit = args[4].intVal;
        const float cx = center->GetX(), cy = center->GetY();
        const float rSq = radius * radius;
        git->second->unitHandles.clear();
        if (!sim) return JassValue::Null();
        int added = 0;
        for (CUnit* unit : sim->GetUnits()) {
            if (!unit) continue;
            const float dx = unit->GetX() - cx, dy = unit->GetY() - cy;
            if (dx * dx + dy * dy > rSq) continue;
            const int unitHandle = data->AllocHandle(JassHandleInfo::UNIT, unit->GetHandleId());
            bool accepted = true;
            if (!filterFunc.empty() && vmPtr) {
                const int prev = data->filterUnitHandle;
                data->filterUnitHandle = unitHandle;
                accepted = JassValueToBool(CallSyncFunction(vmPtr, filterFunc));
                data->filterUnitHandle = prev;
            }
            if (accepted) {
                git->second->unitHandles.push_back(unitHandle);
                if (++added == countLimit) break;
            }
        }
        data->filterUnitHandle = -1;
        return JassValue::Null();
    };
    vm_.RegisterNative("GroupEnumUnitsInRangeOfUnit", enumInRangeOfUnit);
    vm_.RegisterNative("GroupEnumUnitsInRangeOfUnitCounted", enumInRangeOfUnit);
}
void JassNatives::RegisterForceNatives() {
    vm_.RegisterNative("CreateForce", [data = data_](const std::vector<JassValue>&) -> JassValue { if (std::getenv("WARSMASH_COUNT_FORCES")) { static int n = 0; std::cerr << "[FORCE] create #" << ++n << std::endl; } auto force = std::make_shared<JassForce>(); int id = data->nextForceId++; data->forces[id] = force; return JassValue::Int(data->AllocHandle(JassHandleInfo::FORCE, id)); });
    vm_.RegisterNative("DestroyForce", [data = data_](const std::vector<JassValue>& args) -> JassValue { if (args.empty()) return JassValue::Null(); int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::FORCE); if (simId >= 0) { data->forces.erase(simId); data->FreeHandle(args[0].intVal); } return JassValue::Null(); });
    vm_.RegisterNative("ForceAddPlayer", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::FORCE);
        if (simId < 0) return JassValue::Null();
        auto fit = data->forces.find(simId);
        if (fit == data->forces.end()) return JassValue::Null();
        int pSim = data->GetSimHandle(args[1].intVal, JassHandleInfo::PLAYER);
        if (pSim >= 0 && std::find(fit->second->playerIds.begin(), fit->second->playerIds.end(), pSim) == fit->second->playerIds.end())
            fit->second->playerIds.push_back(pSim);
        return JassValue::Null();
    });
    vm_.RegisterNative("ForceRemovePlayer", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::FORCE);
        if (simId < 0) return JassValue::Null();
        auto fit = data->forces.find(simId);
        if (fit == data->forces.end()) return JassValue::Null();
        int pSim = data->GetSimHandle(args[1].intVal, JassHandleInfo::PLAYER);
        auto& v = fit->second->playerIds;
        v.erase(std::remove(v.begin(), v.end(), pSim), v.end());
        return JassValue::Null();
    });
    vm_.RegisterNative("ForceClear", [data = data_](const std::vector<JassValue>& args) -> JassValue { if (args.empty()) return JassValue::Null(); int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::FORCE); if (simId >= 0) { auto fit = data->forces.find(simId); if (fit != data->forces.end()) fit->second->playerIds.clear(); } return JassValue::Null(); });
    auto addPlayerToForce = [data = data_](JassForce& force, int playerId) { if (std::find(force.playerIds.begin(), force.playerIds.end(), playerId) == force.playerIds.end()) { force.playerIds.push_back(playerId); } };
    auto acceptPlayer = [data = data_, vmPtr = vmPtr_](int playerId, const std::string& filterFunc) { if (filterFunc.empty() || !vmPtr) return true; const int previousFilterPlayer = data->filterPlayerHandle; data->filterPlayerHandle = data->AllocHandle(JassHandleInfo::PLAYER, playerId); const bool accepted = JassValueToBool(CallSyncFunction(vmPtr, filterFunc)); data->filterPlayerHandle = previousFilterPlayer; return accepted; };
    vm_.RegisterNative("ForceEnumPlayers", [data = data_, sim = sim_, addPlayerToForce, acceptPlayer](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Null();
        int forceId = data->GetSimHandle(args[0].intVal, JassHandleInfo::FORCE);
        if (forceId < 0) return JassValue::Null();
        auto fit = data->forces.find(forceId);
        if (fit == data->forces.end()) return JassValue::Null();
        const std::string filterFunc = (args.size() > 1 && args[1].type == JassValue::STRING) ? args[1].strVal : "";
        const int simCount = sim ? sim->GetPlayerCount() : 0;
        const int playerCount = std::max(simCount, 16);
        for (int playerId = 0; playerId < playerCount; ++playerId) {
            if (acceptPlayer(playerId, filterFunc)) { addPlayerToForce(*fit->second, playerId); }
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("ForceEnumPlayersCounted", [data = data_, sim = sim_, addPlayerToForce, acceptPlayer](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3) return JassValue::Null();
        int forceId = data->GetSimHandle(args[0].intVal, JassHandleInfo::FORCE);
        if (forceId < 0) return JassValue::Null();
        auto fit = data->forces.find(forceId);
        if (fit == data->forces.end()) return JassValue::Null();
        const std::string filterFunc = (args[1].type == JassValue::STRING) ? args[1].strVal : "";
        const int limit = std::max(0, args[2].intVal);
        const int simCount = sim ? sim->GetPlayerCount() : 0;
        const int playerCount = std::max(simCount, 16);
        int acceptedCount = 0;
        for (int playerId = 0; playerId < playerCount && acceptedCount < limit; ++playerId) {
            if (acceptPlayer(playerId, filterFunc)) {
                addPlayerToForce(*fit->second, playerId);
                ++acceptedCount;
            }
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("ForceEnumAllies", [data = data_, sim = sim_, addPlayerToForce, acceptPlayer](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !sim) return JassValue::Null();
        int forceId = data->GetSimHandle(args[0].intVal, JassHandleInfo::FORCE);
        int sourcePlayerId = ResolvePlayerId(data, args[1].intVal);
        if (forceId < 0 || sourcePlayerId < 0) return JassValue::Null();
        auto fit = data->forces.find(forceId);
        CPlayer* sourcePlayer = sim->GetPlayer(sourcePlayerId);
        if (fit == data->forces.end() || !sourcePlayer) return JassValue::Null();
        const std::string filterFunc = (args.size() > 2 && args[2].type == JassValue::STRING) ? args[2].strVal : "";
        for (int playerId = 0; playerId < sim->GetPlayerCount(); ++playerId) {
            CPlayer* player = sim->GetPlayer(playerId);
            if (player && sourcePlayer->HasAlliance(*player, CAllianceType::PASSIVE) && acceptPlayer(playerId, filterFunc)) { addPlayerToForce(*fit->second, playerId); }
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("ForceEnumEnemies", [data = data_, sim = sim_, addPlayerToForce, acceptPlayer](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !sim) return JassValue::Null();
        int forceId = data->GetSimHandle(args[0].intVal, JassHandleInfo::FORCE);
        int sourcePlayerId = ResolvePlayerId(data, args[1].intVal);
        if (forceId < 0 || sourcePlayerId < 0) return JassValue::Null();
        auto fit = data->forces.find(forceId);
        CPlayer* sourcePlayer = sim->GetPlayer(sourcePlayerId);
        if (fit == data->forces.end() || !sourcePlayer) return JassValue::Null();
        const std::string filterFunc = (args.size() > 2 && args[2].type == JassValue::STRING) ? args[2].strVal : "";
        for (int playerId = 0; playerId < sim->GetPlayerCount(); ++playerId) {
            CPlayer* player = sim->GetPlayer(playerId);
            if (player && !sourcePlayer->HasAlliance(*player, CAllianceType::PASSIVE) && acceptPlayer(playerId, filterFunc)) { addPlayerToForce(*fit->second, playerId); }
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("ForForce", [data = data_, vmPtr = vmPtr_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        int forceId = data->GetSimHandle(args[0].intVal, JassHandleInfo::FORCE);
        if (forceId < 0) return JassValue::Null();
        auto fit = data->forces.find(forceId);
        if (fit == data->forces.end()) return JassValue::Null();
        const std::string& funcName = args[1].strVal;
        if (funcName.empty()) return JassValue::Null();
        const int previousEnumPlayer = data->enumPlayerHandle;
        const std::vector<int> snapshot = fit->second->playerIds;
        for (int playerId : snapshot) {
            data->enumPlayerHandle = data->AllocHandle(JassHandleInfo::PLAYER, playerId);
            ExecuteSyncFunction(vmPtr, funcName);
        }
        data->enumPlayerHandle = previousEnumPlayer;
        return JassValue::Null();
    });
    vm_.RegisterNative("CountPlayersInForce", [data = data_](const std::vector<JassValue>& args) -> JassValue { if (args.empty()) return JassValue::Int(0); int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::FORCE); if (simId < 0) return JassValue::Int(0); auto fit = data->forces.find(simId); if (fit == data->forces.end()) return JassValue::Int(0); return JassValue::Int((int)fit->second->playerIds.size()); });
}
void JassNatives::RegisterUnitNatives() {
    vm_.RegisterNative("ConvertPrimaryTag", [data = data_](const std::vector<JassValue>& args) -> JassValue { if (args.empty()) return JassValue::Null(); const int ordinal = args[0].intVal; if (!JassAnimationPrimaryTagName(ordinal)[0]) return JassValue::Null(); return JassValue::Int(data->AllocHandle(JassHandleInfo::PRIMARY_TAG, ordinal)); });
    vm_.RegisterNative("ConvertSecondaryTag", [data = data_](const std::vector<JassValue>& args) -> JassValue { if (args.empty()) return JassValue::Null(); const int ordinal = args[0].intVal; if (!JassAnimationSecondaryTagName(ordinal)[0]) return JassValue::Null(); return JassValue::Int(data->AllocHandle(JassHandleInfo::SECONDARY_TAG, ordinal)); });
    vm_.RegisterNative("CreatePrimaryTags", [data = data_](const std::vector<JassValue>&) -> JassValue { const int id = data->nextPrimaryTagSetId++; data->primaryTagSets[id] = {}; return JassValue::Int(data->AllocHandle(JassHandleInfo::PRIMARY_TAGS, id)); });
    vm_.RegisterNative("CreateSecondaryTags", [data = data_](const std::vector<JassValue>&) -> JassValue { const int id = data->nextSecondaryTagSetId++; data->secondaryTagSets[id] = {}; return JassValue::Int(data->AllocHandle(JassHandleInfo::SECONDARY_TAGS, id)); });
    vm_.RegisterNative("PrimaryTagsAdd", [data = data_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2) return JassValue::Bool(false); const int setId = data->GetSimHandle(args[0].intVal, JassHandleInfo::PRIMARY_TAGS); const int tag = data->GetSimHandle(args[1].intVal, JassHandleInfo::PRIMARY_TAG); if (setId < 0 || tag < 0) return JassValue::Bool(false); return JassValue::Bool(data->primaryTagSets[setId].insert(tag).second); });
    vm_.RegisterNative("SecondaryTagsAdd", [data = data_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2) return JassValue::Bool(false); const int setId = data->GetSimHandle(args[0].intVal, JassHandleInfo::SECONDARY_TAGS); const int tag = data->GetSimHandle(args[1].intVal, JassHandleInfo::SECONDARY_TAG); if (setId < 0 || tag < 0) return JassValue::Bool(false); return JassValue::Bool(data->secondaryTagSets[setId].insert(tag).second); });
    vm_.RegisterNative("PrimaryTagsRemove", [data = data_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2) return JassValue::Bool(false); const int setId = data->GetSimHandle(args[0].intVal, JassHandleInfo::PRIMARY_TAGS); const int tag = data->GetSimHandle(args[1].intVal, JassHandleInfo::PRIMARY_TAG); if (setId < 0 || tag < 0) return JassValue::Bool(false); return JassValue::Bool(data->primaryTagSets[setId].erase(tag) > 0); });
    vm_.RegisterNative("SecondaryTagsRemove", [data = data_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2) return JassValue::Bool(false); const int setId = data->GetSimHandle(args[0].intVal, JassHandleInfo::SECONDARY_TAGS); const int tag = data->GetSimHandle(args[1].intVal, JassHandleInfo::SECONDARY_TAG); if (setId < 0 || tag < 0) return JassValue::Bool(false); return JassValue::Bool(data->secondaryTagSets[setId].erase(tag) > 0); });
    vm_.RegisterNative("PrimaryTagsContains", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Bool(false);
        const int setId = data->GetSimHandle(args[0].intVal, JassHandleInfo::PRIMARY_TAGS);
        const int tag = data->GetSimHandle(args[1].intVal, JassHandleInfo::PRIMARY_TAG);
        if (setId < 0 || tag < 0) return JassValue::Bool(false);
        auto it = data->primaryTagSets.find(setId);
        return JassValue::Bool(it != data->primaryTagSets.end() && it->second.count(tag) > 0);
    });
    vm_.RegisterNative("SecondaryTagsContains", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Bool(false);
        const int setId = data->GetSimHandle(args[0].intVal, JassHandleInfo::SECONDARY_TAGS);
        const int tag = data->GetSimHandle(args[1].intVal, JassHandleInfo::SECONDARY_TAG);
        if (setId < 0 || tag < 0) return JassValue::Bool(false);
        auto it = data->secondaryTagSets.find(setId);
        return JassValue::Bool(it != data->secondaryTagSets.end() && it->second.count(tag) > 0);
    });
    vm_.RegisterNative("PrimaryTagsSize", [data = data_](const std::vector<JassValue>& args) -> JassValue { if (args.empty()) return JassValue::Int(0); const int setId = data->GetSimHandle(args[0].intVal, JassHandleInfo::PRIMARY_TAGS); auto it = data->primaryTagSets.find(setId); return JassValue::Int(it != data->primaryTagSets.end() ? (int)it->second.size() : 0); });
    vm_.RegisterNative("SecondaryTagsSize", [data = data_](const std::vector<JassValue>& args) -> JassValue { if (args.empty()) return JassValue::Int(0); const int setId = data->GetSimHandle(args[0].intVal, JassHandleInfo::SECONDARY_TAGS); auto it = data->secondaryTagSets.find(setId); return JassValue::Int(it != data->secondaryTagSets.end() ? (int)it->second.size() : 0); });
    vm_.RegisterNative("PrimaryTagsAny", [data = data_](const std::vector<JassValue>& args) -> JassValue { if (args.empty()) return JassValue::Null(); const int setId = data->GetSimHandle(args[0].intVal, JassHandleInfo::PRIMARY_TAGS); auto it = data->primaryTagSets.find(setId); if (it == data->primaryTagSets.end() || it->second.empty()) return JassValue::Null(); return JassValue::Int(data->AllocHandle(JassHandleInfo::PRIMARY_TAG, *it->second.begin())); });
    vm_.RegisterNative("SecondaryTagsAny", [data = data_](const std::vector<JassValue>& args) -> JassValue { if (args.empty()) return JassValue::Null(); const int setId = data->GetSimHandle(args[0].intVal, JassHandleInfo::SECONDARY_TAGS); auto it = data->secondaryTagSets.find(setId); if (it == data->secondaryTagSets.end() || it->second.empty()) return JassValue::Null(); return JassValue::Int(data->AllocHandle(JassHandleInfo::SECONDARY_TAG, *it->second.begin())); });
    vm_.RegisterNative("DestroyPrimaryTags", [data = data_](const std::vector<JassValue>& args) -> JassValue { if (args.empty()) return JassValue::Null(); const int setId = data->GetSimHandle(args[0].intVal, JassHandleInfo::PRIMARY_TAGS); if (setId >= 0) data->primaryTagSets.erase(setId); data->FreeHandle(args[0].intVal); return JassValue::Null(); });
    vm_.RegisterNative("DestroySecondaryTags", [data = data_](const std::vector<JassValue>& args) -> JassValue { if (args.empty()) return JassValue::Null(); const int setId = data->GetSimHandle(args[0].intVal, JassHandleInfo::SECONDARY_TAGS); if (setId >= 0) data->secondaryTagSets.erase(setId); data->FreeHandle(args[0].intVal); return JassValue::Null(); });
    vm_.RegisterNative("PopulateTags", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3) return JassValue::Null();
        const int primarySetId = data->GetSimHandle(args[0].intVal, JassHandleInfo::PRIMARY_TAGS);
        const int secondarySetId = data->GetSimHandle(args[1].intVal, JassHandleInfo::SECONDARY_TAGS);
        if (primarySetId < 0 || secondarySetId < 0) return JassValue::Null();
        std::vector<int> primaryTags;
        std::vector<int> secondaryTags;
        JassPopulateAnimationTagsFromSelector(args[2].strVal, primaryTags, secondaryTags);
        auto& primarySet = data->primaryTagSets[primarySetId];
        auto& secondarySet = data->secondaryTagSets[secondarySetId];
        primarySet.clear();
        secondarySet.clear();
        primarySet.insert(primaryTags.begin(), primaryTags.end());
        secondarySet.insert(secondaryTags.begin(), secondaryTags.end());
        return JassValue::Null();
    });
    auto createUnitByTypeId = [data = data_, sim = sim_](int playerArg, War3ID typeId,
                                                          float x, float y, float facing,
                                                          bool addTechUnlocked = true) -> JassValue {
        if (!sim) return JassValue::Null();
        const int playerId = ResolvePlayerId(data, playerArg);
        if (playerId < 0) return JassValue::Null();
        CUnit* unit = sim->CreateUnit(typeId, playerId, x, y, facing);
        if (!unit) return JassValue::Null();
        if (addTechUnlocked) {
            if (CPlayer* player = sim->GetPlayer(playerId)) { player->AddTechUnlocked(*sim, typeId); }
        }
        return JassValue::Int(data->AllocHandle(JassHandleInfo::UNIT, unit->GetHandleId()));
    };
    vm_.RegisterNative("CreateUnit", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (!sim || args.size() < 5) return JassValue::Null();
        int playerId = ResolvePlayerId(data, args[0].intVal);
        if (playerId < 0) return JassValue::Null();
        int unitTypeId = args[1].intVal;
        float x = args[2].AsReal(), y = args[3].AsReal(), facing = args[4].AsReal();
        War3ID w3id((uint32_t)unitTypeId);
        CUnit* unit = sim->CreateUnit(w3id, playerId, x, y, facing);
        if (unit) {
            if (CPlayer* player = sim->GetPlayer(playerId)) { player->AddTechUnlocked(*sim, w3id); }
        }
        if (unit) return JassValue::Int(data->AllocHandle(JassHandleInfo::UNIT, unit->GetHandleId()));
        return JassValue::Null();
    });
    vm_.RegisterNative("CreateUnitByName", [sim = sim_, createUnitByTypeId](const std::vector<JassValue>& args) -> JassValue { if (!sim || args.size() < 5) return JassValue::Null(); const int typeId = UnitIdFromLegacyName(sim, args[1].strVal); if (typeId == 0) return JassValue::Null(); return createUnitByTypeId(args[0].intVal, War3ID((uint32_t)typeId), args[2].AsReal(), args[3].AsReal(), args[4].AsReal()); });
    vm_.RegisterNative("CreateUnitAtLoc", [data = data_, createUnitByTypeId](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 4) return JassValue::Null();
        float x = 0.0f;
        float y = 0.0f;
        const int locId = data->GetSimHandle(args[2].intVal, JassHandleInfo::LOCATION);
        if (locId >= 0) {
            auto locIt = data->locations.find(locId);
            if (locIt != data->locations.end()) {
                x = locIt->second->x;
                y = locIt->second->y;
            }
        }
        return createUnitByTypeId(args[0].intVal, War3ID((uint32_t)args[1].intVal), x, y, args[3].AsReal());
    });
    vm_.RegisterNative("CreateUnitAtLocByName", [data = data_, sim = sim_, createUnitByTypeId](const std::vector<JassValue>& args) -> JassValue {
        if (!sim || args.size() < 4) return JassValue::Null();
        const int typeId = UnitIdFromLegacyName(sim, args[1].strVal);
        if (typeId == 0) return JassValue::Null();
        float x = 0.0f;
        float y = 0.0f;
        const int locId = data->GetSimHandle(args[2].intVal, JassHandleInfo::LOCATION);
        if (locId >= 0) {
            auto locIt = data->locations.find(locId);
            if (locIt != data->locations.end()) {
                x = locIt->second->x;
                y = locIt->second->y;
            }
        }
        return createUnitByTypeId(args[0].intVal, War3ID((uint32_t)typeId), x, y, args[3].realVal);
    });
    vm_.RegisterNative("CreateCorpse", [data = data_, sim = sim_, createUnitByTypeId](const std::vector<JassValue>& args) -> JassValue { if (!sim || args.size() < 5) return JassValue::Null(); JassValue handle = createUnitByTypeId(args[0].intVal, War3ID((uint32_t)args[1].intVal), args[2].realVal, args[3].realVal, args[4].realVal, false); int simId = data->GetSimHandle(handle.intVal, JassHandleInfo::UNIT); if (simId >= 0) { CUnit* unit = sim->GetUnit(simId); if (unit) unit->SetLife(*sim, 0.0f); } return handle; });
    vm_.RegisterNative("BlzCreateUnitWithSkin", [createUnitByTypeId](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 6) return JassValue::Null(); return createUnitByTypeId(args[0].intVal, War3ID((uint32_t)args[1].intVal), args[2].realVal, args[3].realVal, args[4].realVal, false); });
    vm_.RegisterNative("RemoveUnit", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (!sim || args.empty()) return JassValue::Null(); int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT); if (simId >= 0) { CUnit* unit = sim->GetUnit(simId); if (unit) sim->RemoveUnit(unit); data->FreeHandle(args[0].intVal); } return JassValue::Null(); });
    vm_.RegisterNative("KillUnit", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (!sim || args.empty()) return JassValue::Null(); int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT); if (simId >= 0) { CUnit* unit = sim->GetUnit(simId); if (unit) unit->SetLife(*sim, 0.0f); } return JassValue::Null(); });
    vm_.RegisterNative("SetUnitLookAt", [](const std::vector<JassValue>&) { return JassValue::Null(); });
    vm_.RegisterNative("ResetUnitLookAt", [](const std::vector<JassValue>&) { return JassValue::Null(); });
    vm_.RegisterNative("UnitResetCooldown", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty() || !data) return JassValue::Null();
        const int uSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (uSim < 0) return JassValue::Null();
        for (auto it = data->abilityCooldowns.begin(); it != data->abilityCooldowns.end();) { if ((int)(it->first >> 32) == uSim) { it = data->abilityCooldowns.erase(it); } else { ++it; } }
        if (sim) {
            if (CUnit* unit = sim->GetUnit(uSim)) {
                for (CAbility* ability : unit->GetAbilities()) {
                    if (ability) ability->ResetCooldown();
                }
            }
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("SetUnitOwner", [data = data_, sim = sim_, this](const std::vector<JassValue>& args) -> JassValue {
        if (!sim || args.size() < 2) return JassValue::Null();
        const int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        const int newOwner = ResolvePlayerId(data, args[1].intVal);
        if (simId >= 0 && newOwner >= 0 && sim->GetPlayer(newOwner)) {
            if (CUnit* unit = sim->GetUnit(simId)) {
                const int prevOwner = unit->GetPlayerIndex();
                unit->SetPlayerIndex(newOwner);
                if (prevOwner != newOwner) { FireChangeOwnerEvent(simId, prevOwner); }
            }
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("RemoveGuardPosition", [](const std::vector<JassValue>&) { return JassValue::Null(); });
    vm_.RegisterNative("RecycleGuardPosition", [](const std::vector<JassValue>&) { return JassValue::Null(); });
    vm_.RegisterNative("RemoveAllGuardPositions", [](const std::vector<JassValue>&) { return JassValue::Null(); });
    vm_.RegisterNative("GetUnitTypeId", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.empty()) return JassValue::Int(0); int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT); if (simId >= 0) { CUnit* unit = sim->GetUnit(simId); if (unit) return JassValue::Int(unit->GetTypeId().value); } return JassValue::Int(0); });
    vm_.RegisterNative("GetUnitState", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Real(0);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId >= 0) {
            CUnit* unit = sim->GetUnit(simId);
            if (unit) {
                switch (args[1].intVal) {
                    case 0: return JassValue::Real(unit->GetLife());
                    case 1: return JassValue::Real(unit->GetMaxLife());
                    case 2: return JassValue::Real(unit->GetMana());
                    case 3: return JassValue::Real((float)unit->GetMaxMana());
                }
            }
        }
        return JassValue::Real(0);
    });
    vm_.RegisterNative("SetUnitState", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 3) return JassValue::Null(); int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT); if (simId >= 0) { CUnit* unit = sim->GetUnit(simId); if (unit) { switch (args[1].intVal) { case 0: unit->SetLife(*sim, args[2].realVal); break; case 2: unit->SetMana(args[2].realVal); break; } } } return JassValue::Null(); });
    vm_.RegisterNative("GetUnitX", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.empty()) return JassValue::Real(0); int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT); if (simId >= 0) { CUnit* unit = sim->GetUnit(simId); if (unit) return JassValue::Real(unit->GetX()); } return JassValue::Real(0); });
    vm_.RegisterNative("GetUnitY", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.empty()) return JassValue::Real(0); int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT); if (simId >= 0) { CUnit* unit = sim->GetUnit(simId); if (unit) return JassValue::Real(unit->GetY()); } return JassValue::Real(0); });
    vm_.RegisterNative("GetUnitFacing", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.empty()) return JassValue::Real(0); int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT); if (simId >= 0) { CUnit* unit = sim->GetUnit(simId); if (unit) return JassValue::Real(unit->GetFacing()); } return JassValue::Real(0); });
    vm_.RegisterNative("GetUnitLoc", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        auto loc = std::make_shared<JassLocation>();
        if (!args.empty() && sim) {
            int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
            if (simId >= 0) {
                CUnit* unit = sim->GetUnit(simId);
                if (unit) {
                    loc->x = unit->GetX();
                    loc->y = unit->GetY();
                }
            }
        }
        int id = data->nextLocId++;
        data->locations[id] = loc;
        return JassValue::Int(data->AllocHandle(JassHandleInfo::LOCATION, id));
    });
    vm_.RegisterNative("SetUnitUserData", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2) return JassValue::Null(); int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT); if (simId >= 0) { data->unitUserData[simId] = args[1].intVal; } (void)sim; return JassValue::Null(); });
    vm_.RegisterNative("GetUnitUserData", [data = data_](const std::vector<JassValue>& args) -> JassValue { if (args.empty()) return JassValue::Int(0); int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT); if (simId < 0) return JassValue::Int(0); auto it = data->unitUserData.find(simId); return JassValue::Int(it != data->unitUserData.end() ? it->second : 0); });
    vm_.RegisterNative("SetUnitX", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2 || !sim) return JassValue::Null(); int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT); if (simId >= 0) { CUnit* unit = sim->GetUnit(simId); if (unit) unit->SetPositionAndCheckUnstuck(args[1].realVal, unit->GetY(), *sim); } return JassValue::Null(); });
    vm_.RegisterNative("SetUnitY", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2 || !sim) return JassValue::Null(); int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT); if (simId >= 0) { CUnit* unit = sim->GetUnit(simId); if (unit) unit->SetPositionAndCheckUnstuck(unit->GetX(), args[1].realVal, *sim); } return JassValue::Null(); });
    vm_.RegisterNative("SetUnitPosition", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 3 || !sim) return JassValue::Null(); int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT); if (simId >= 0) { CUnit* unit = sim->GetUnit(simId); if (unit) unit->SetPositionAndCheckUnstuck(args[1].realVal, args[2].realVal, *sim); } return JassValue::Null(); });
    vm_.RegisterNative("SetUnitPositionLoc", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !sim) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        int locSim = data->GetSimHandle(args[1].intVal, JassHandleInfo::LOCATION);
        if (simId >= 0 && locSim >= 0) {
            CUnit* unit = sim->GetUnit(simId);
            auto locIt = data->locations.find(locSim);
            if (unit && locIt != data->locations.end()) { unit->SetPositionAndCheckUnstuck(locIt->second->x, locIt->second->y, *sim); }
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("UnitDamageTarget", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3 || !sim) return JassValue::Bool(false);
        int srcSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        int tgtSim = data->GetSimHandle(args[1].intVal, JassHandleInfo::UNIT);
        if (srcSim >= 0 && tgtSim >= 0) {
            CUnit* src = sim->GetUnit(srcSim);
            CWidget* tgt = sim->GetUnit(tgtSim);
            if (src && tgt) {
                float dmg = args[2].realVal;
                bool isAttack = args.size() > 3 ? JassValueToBool(args[3]) : true;
                bool isRanged = args.size() > 4 ? JassValueToBool(args[4]) : false;
                CAttackType attackType = CAttackType::NORMAL;
                if (args.size() > 5) {
                    switch (args[5].intVal) {
                        case 0: attackType = CAttackType::SPELLS; break;
                        case 1: attackType = CAttackType::NORMAL; break;
                        case 2: attackType = CAttackType::PIERCE; break;
                        case 3: attackType = CAttackType::SIEGE; break;
                        case 4: attackType = CAttackType::MAGIC; break;
                        case 5: attackType = CAttackType::CHAOS; break;
                        case 6: attackType = CAttackType::HERO; break;
                        default: attackType = CAttackType::NORMAL; break;
                    }
                }
                CDamageType damageType = CDamageType::NORMAL;
                if (args.size() > 6) {
                    switch (args[6].intVal) {
                        case 4: damageType = CDamageType::NORMAL; break;
                        case 5: damageType = CDamageType::ENHANCED; break;
                        case 8: damageType = CDamageType::FIRE; break;
                        case 9: damageType = CDamageType::COLD; break;
                        case 10: damageType = CDamageType::LIGHTNING; break;
                        case 11: damageType = CDamageType::POISON; break;
                        case 12: damageType = CDamageType::DISEASE; break;
                        case 13: damageType = CDamageType::DIVINE; break;
                        case 14: damageType = CDamageType::MAGIC; break;
                        case 15: damageType = CDamageType::SONIC; break;
                        case 16: damageType = CDamageType::ACID; break;
                        case 17: damageType = CDamageType::FORCE; break;
                        case 18: damageType = CDamageType::DEATH; break;
                        case 19: damageType = CDamageType::MIND; break;
                        case 20: damageType = CDamageType::PLANT; break;
                        case 21: damageType = CDamageType::DEFENSIVE; break;
                        case 22: damageType = CDamageType::DEMOLITION; break;
                        case 23: damageType = CDamageType::SLOW_POISON; break;
                        case 24: damageType = CDamageType::SPIRIT_LINK; break;
                        case 25: damageType = CDamageType::SHADOW_STRIKE; break;
                        case 26: damageType = CDamageType::UNIVERSAL; break;
                        default: damageType = CDamageType::NORMAL; break;
                    }
                }
                auto* combat = sim->GetCombatSystem();
                if (combat) {
                    auto* atkType = src->GetUnitType();
                    int dice = 0, sides = 0;
                    if (atkType && !atkType->GetAttacks().empty()) {
                        dice = atkType->GetAttacks()[0].damageDice;
                        sides = atkType->GetAttacks()[0].damageSidesPerDie;
                    }
                    combat->ApplyDamage(*sim, src, tgt, isAttack, isRanged,
                                        attackType, damageType,
                                        "", dmg, 0, dice, sides);
                } else {
                    tgt->Damage(*sim, src, isAttack, isRanged, attackType, damageType, "", dmg);
                }
                return JassValue::Bool(true);
            }
        }
        return JassValue::Bool(false);
    });
    vm_.RegisterNative("IssuePointOrder", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 4 || !sim) return JassValue::Bool(false);
        int srcSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (srcSim < 0) return JassValue::Bool(false);
        CUnit* src = sim->GetUnit(srcSim);
        if (!src) return JassValue::Bool(false);
        std::string orderStr = args[1].strVal;
        float x = args[2].realVal, y = args[3].realVal;
        int orderId = ParseOrderString(orderStr);
        if (orderId > 0)
            src->IssueOrder(std::make_unique<COrderTargetPoint>(orderId, x, y));
        return JassValue::Bool(true);
    });
    vm_.RegisterNative("IssueImmediateOrder", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !sim) return JassValue::Bool(false);
        int srcSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (srcSim < 0) return JassValue::Bool(false);
        CUnit* src = sim->GetUnit(srcSim);
        if (!src) return JassValue::Bool(false);
        std::string orderStr = args[1].strVal;
        int orderId = ParseOrderString(orderStr);
        if (orderId > 0)
            src->IssueOrder(std::make_unique<COrderNoTarget>(orderId));
        return JassValue::Bool(true);
    });
    vm_.RegisterNative("IssueTargetOrder", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3 || !sim) return JassValue::Bool(false);
        int srcSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (srcSim < 0) return JassValue::Bool(false);
        CUnit* src = sim->GetUnit(srcSim);
        if (!src) return JassValue::Bool(false);
        int orderId = ParseOrderString(args[1].strVal);
        if (orderId <= 0) return JassValue::Bool(false);
        CWidget* tgt = ResolveOrderTargetWidget(data, sim, args[2].intVal);
        if (!tgt) return JassValue::Bool(false);
        src->IssueOrder(std::make_unique<COrderTargetWidget>(orderId, tgt));
        return JassValue::Bool(true);
    });
    vm_.RegisterNative("IssuePointOrderLoc", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3 || !sim) return JassValue::Bool(false);
        int srcSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (srcSim < 0) return JassValue::Bool(false);
        int orderId = ParseOrderString(args[1].strVal);
        if (orderId <= 0) return JassValue::Bool(false);
        int locSim = data->GetSimHandle(args[2].intVal, JassHandleInfo::LOCATION);
        if (locSim < 0) return JassValue::Bool(false);
        auto locIt = data->locations.find(locSim);
        if (locIt == data->locations.end()) return JassValue::Bool(false);
        CUnit* src = sim->GetUnit(srcSim);
        if (!src) return JassValue::Bool(false);
        src->IssueOrder(std::make_unique<COrderTargetPoint>(orderId, locIt->second->x, locIt->second->y));
        return JassValue::Bool(true);
    });
    vm_.RegisterNative("IssueBuildOrder", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 4 || !sim) return JassValue::Bool(false);
        int srcSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (srcSim < 0) return JassValue::Bool(false);
        CUnit* src = sim->GetUnit(srcSim);
        if (!src) return JassValue::Bool(false);
        War3ID w3id((uint32_t)args[1].intVal);
        src->IssueOrder(std::make_unique<COrderTargetPoint>(w3id.value, args[2].realVal, args[3].realVal));
        return JassValue::Bool(true);
    });
    vm_.RegisterNative("SetUnitFacing", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2) return JassValue::Null(); int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT); if (simId >= 0) { CUnit* unit = sim->GetUnit(simId); if (unit) unit->SetFacing(args[1].realVal); } return JassValue::Null(); });
    vm_.RegisterNative("SetUnitFacingTimed", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2) return JassValue::Null(); int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT); if (simId >= 0) { CUnit* unit = sim->GetUnit(simId); if (unit) unit->SetFacing(args[1].realVal); } return JassValue::Null(); });
    vm_.RegisterNative("SetUnitAnimation", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2) return JassValue::Null(); int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT); if (simId < 0 || !sim) return JassValue::Null(); CUnit* unit = sim->GetUnit(simId); if (!unit) return JassValue::Null(); if (data->unitSetAnimation) data->unitSetAnimation(simId, args[1].strVal); return JassValue::Null(); });
    vm_.RegisterNative("SetUnitAnimationByIndex", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2) return JassValue::Null(); int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT); if (simId < 0 || !sim) return JassValue::Null(); if (data->unitSetAnimationByIndex) data->unitSetAnimationByIndex(simId, args[1].intVal); return JassValue::Null(); });
    vm_.RegisterNative("SetUnitAnimationWithRarity", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2) return JassValue::Null(); int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT); if (simId < 0 || !sim) return JassValue::Null(); if (data->unitSetAnimation) data->unitSetAnimation(simId, args[1].strVal); return JassValue::Null(); });
    vm_.RegisterNative("QueueUnitAnimation", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2) return JassValue::Null(); int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT); if (simId < 0 || !sim) return JassValue::Null(); if (data->unitQueueAnimation) data->unitQueueAnimation(simId, args[1].strVal); else if (data->unitSetAnimation) data->unitSetAnimation(simId, args[1].strVal); return JassValue::Null(); });
    vm_.RegisterNative("SetUnitAnimationByTag", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 6) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId < 0 || !sim || !sim->GetUnit(simId)) return JassValue::Null();
        const std::string primary = ResolvePrimaryTagName(data, args[2].intVal);
        std::vector<std::string> secondary = ResolveSecondaryTagNames(data, args[3].intVal);
        if (data->unitSetAnimationByTags) { data->unitSetAnimationByTags(simId, primary, secondary, args[4].realVal, 0.0f, JassValueToBool(args[1])); }
        return JassValue::Null();
    });
    vm_.RegisterNative("SetUnitAnimationByTagWithDuration", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 6) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId < 0 || !sim || !sim->GetUnit(simId)) return JassValue::Null();
        const std::string primary = ResolvePrimaryTagName(data, args[2].intVal);
        std::vector<std::string> secondary = ResolveSecondaryTagNames(data, args[3].intVal);
        if (data->unitSetAnimationByTags) { data->unitSetAnimationByTags(simId, primary, secondary, 1.0f, args[4].realVal, JassValueToBool(args[1])); }
        return JassValue::Null();
    });
    vm_.RegisterNative("SetUnitAnimationToWalk", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId < 0 || !sim || !sim->GetUnit(simId)) return JassValue::Null();
        if (data->unitSetAnimationByTags) {
            data->unitSetAnimationByTags(simId, "walk", {}, 1.0f, 0.0f, JassValueToBool(args[1]));
        } else if (data->unitSetAnimation) { data->unitSetAnimation(simId, "walk"); }
        return JassValue::Null();
    });
    vm_.RegisterNative("SetUnitAnimationByIndexEx", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 5) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId < 0 || !sim || !sim->GetUnit(simId)) return JassValue::Null();
        if (data->unitSetAnimationByIndexEx) data->unitSetAnimationByIndexEx(simId, args[2].intVal, args[3].realVal);
        else if (data->unitSetAnimationByIndex) data->unitSetAnimationByIndex(simId, args[2].intVal);
        return JassValue::Null();
    });
    vm_.RegisterNative("QueueUnitAnimationByTag", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 4) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId < 0 || !sim || !sim->GetUnit(simId)) return JassValue::Null();
        const std::string primary = ResolvePrimaryTagName(data, args[1].intVal);
        std::vector<std::string> secondary = ResolveSecondaryTagNames(data, args[2].intVal);
        if (data->unitQueueAnimationByTags) data->unitQueueAnimationByTags(simId, primary, secondary);
        else if (data->unitSetAnimationByTags) data->unitSetAnimationByTags(simId, primary, secondary, 1.0f, 0.0f, false);
        return JassValue::Null();
    });
    vm_.RegisterNative("AddUnitAnimationProperties", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 3) return JassValue::Null(); int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT); if (simId < 0 || !sim) return JassValue::Null(); if (data->unitAddAnimationProperties) data->unitAddAnimationProperties(simId, args[1].strVal, args[2].boolVal); return JassValue::Null(); });
    vm_.RegisterNative("AddUnitAnimationSecondaryTag", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Bool(false);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId < 0 || !sim || !sim->GetUnit(simId)) return JassValue::Bool(false);
        const int ordinal = data->GetSimHandle(args[1].intVal, JassHandleInfo::SECONDARY_TAG);
        const char* tag = JassAnimationSecondaryTagName(ordinal);
        if (!tag[0]) return JassValue::Bool(false);
        if (data->unitSetAnimationSecondaryTag) return JassValue::Bool(data->unitSetAnimationSecondaryTag(simId, tag, true));
        if (data->unitAddAnimationProperties) data->unitAddAnimationProperties(simId, tag, true);
        return JassValue::Bool(true);
    });
    vm_.RegisterNative("RemoveUnitAnimationSecondaryTag", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Bool(false);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId < 0 || !sim || !sim->GetUnit(simId)) return JassValue::Bool(false);
        const int ordinal = data->GetSimHandle(args[1].intVal, JassHandleInfo::SECONDARY_TAG);
        const char* tag = JassAnimationSecondaryTagName(ordinal);
        if (!tag[0]) return JassValue::Bool(false);
        if (data->unitSetAnimationSecondaryTag) return JassValue::Bool(data->unitSetAnimationSecondaryTag(simId, tag, false));
        if (data->unitAddAnimationProperties) data->unitAddAnimationProperties(simId, tag, false);
        return JassValue::Bool(true);
    });
    vm_.RegisterNative("ForceResetUnitCurrentAnimation", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.empty()) return JassValue::Null(); int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT); if (simId >= 0 && sim && sim->GetUnit(simId) && data->unitForceResetAnimation) { data->unitForceResetAnimation(simId); } return JassValue::Null(); });
    vm_.RegisterNative("SetUnitTimeScale", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2) return JassValue::Null(); int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT); if (simId >= 0 && sim) { CUnit* unit = sim->GetUnit(simId); if (unit) unit->SetTimeScale(args[1].realVal); } return JassValue::Null(); });
    vm_.RegisterNative("SetUnitScale", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2) return JassValue::Null(); int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT); if (simId >= 0 && sim) { CUnit* unit = sim->GetUnit(simId); if (unit) unit->SetScale(args[1].realVal); } return JassValue::Null(); });
    vm_.RegisterNative("SetUnitColor", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2) return JassValue::Null(); int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT); if (simId >= 0 && sim) { CUnit* unit = sim->GetUnit(simId); if (unit) unit->SetPlayerColor(args[1].intVal); } return JassValue::Null(); });
    vm_.RegisterNative("SetUnitVertexColor", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 5) return JassValue::Null(); int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT); if (simId >= 0 && sim) { CUnit* unit = sim->GetUnit(simId); if (unit) unit->SetVertexColor(args[1].intVal, args[2].intVal, args[3].intVal, args[4].intVal); } return JassValue::Null(); });
    vm_.RegisterNative("SetUnitPathing", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2) return JassValue::Null(); int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT); if (simId >= 0 && sim) { CUnit* unit = sim->GetUnit(simId); if (unit) unit->SetPathingEnabled(args[1].boolVal); } return JassValue::Null(); });
    vm_.RegisterNative("PauseUnit", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.empty()) return JassValue::Null(); int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT); if (simId >= 0) { CUnit* unit = sim->GetUnit(simId); if (unit) unit->SetHidden(args.size() > 1 ? args[1].boolVal : true); } return JassValue::Null(); });
    vm_.RegisterNative("ShowUnit", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2) return JassValue::Null(); int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT); if (simId >= 0) { CUnit* unit = sim->GetUnit(simId); if (unit) unit->SetHidden(!args[1].boolVal); } return JassValue::Null(); });
    vm_.RegisterNative("IsUnitHidden", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.empty()) return JassValue::Bool(false); int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT); if (simId >= 0) { CUnit* unit = sim->GetUnit(simId); if (unit) return JassValue::Bool(unit->IsHidden()); } return JassValue::Bool(false); });
    vm_.RegisterNative("IsUnitPaused", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.empty()) return JassValue::Bool(false); int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT); if (simId >= 0) { CUnit* unit = sim->GetUnit(simId); if (unit) return JassValue::Bool(unit->IsHidden()); } return JassValue::Bool(false); });
    vm_.RegisterNative("IsUnitVisible", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !sim) return JassValue::Bool(true);
        const int unitId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        const int player = ResolvePlayerId(data, args[1].intVal);
        CUnit* unit = sim->GetUnit(unitId);
        if (!unit || player < 0) return JassValue::Bool(true);
        if (!data->fog.fogEnabled) return JassValue::Bool(true);
        return JassValue::Bool(sim->GetFogSystem()->IsVisibleTo(player, unit->GetX(), unit->GetY()));
    });
    vm_.RegisterNative("IsUnitInvisible", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !sim) return JassValue::Bool(false);
        const int unitId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        const int player = ResolvePlayerId(data, args[1].intVal);
        CUnit* unit = sim->GetUnit(unitId);
        if (!unit || player < 0 || !unit->IsStealthed()) return JassValue::Bool(false);
        for (CUnit* viewer : sim->GetUnits()) {
            if (!viewer || viewer->IsDead()) continue;
            if (viewer->GetPlayerIndex() != player) continue;
            if (!unit->IsInvisibleTo(viewer)) return JassValue::Bool(false);
        }
        return JassValue::Bool(true);
    });
    vm_.RegisterNative("IsUnitSelected", [data = data_](const std::vector<JassValue>& args) -> JassValue { if (args.empty() || !data) return JassValue::Bool(false); const int uSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT); if (uSim < 0) return JassValue::Bool(false); return JassValue::Bool(data->selectedUnits.count(uSim) > 0); });
    vm_.RegisterNative("IsUnitAlly", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2 || !sim) return JassValue::Bool(false); const int unitId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT); const int playerId = ResolvePlayerId(data, args[1].intVal); CUnit* unit = sim->GetUnit(unitId); return JassValue::Bool(unit && unit->GetPlayerIndex() == playerId); });
    vm_.RegisterNative("IsUnitEnemy", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2 || !sim) return JassValue::Bool(false); const int unitId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT); const int playerId = ResolvePlayerId(data, args[1].intVal); CUnit* unit = sim->GetUnit(unitId); return JassValue::Bool(unit && unit->GetPlayerIndex() != playerId); });
    vm_.RegisterNative("GetUnitRace", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.empty()) return JassValue::Int(0); int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT); if (simId < 0 || !sim) return JassValue::Int(0); CUnit* unit = sim->GetUnit(simId); if (!unit || !unit->GetUnitType()) return JassValue::Int(0); return JassValue::Int(UnitRaceToJassOrdinal(unit->GetUnitType()->GetUnitRace().name)); });
    vm_.RegisterNative("IsUnitRace", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Bool(false);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId < 0 || !sim) return JassValue::Bool(false);
        CUnit* unit = sim->GetUnit(simId);
        if (!unit || !unit->GetUnitType()) return JassValue::Bool(false);
        return JassValue::Bool(UnitRaceToJassOrdinal(unit->GetUnitType()->GetUnitRace().name) == args[1].intVal);
    });
    vm_.RegisterNative("IsUnitIdType", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Bool(false);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId < 0 || !sim) return JassValue::Bool(false);
        CUnit* unit = sim->GetUnit(simId);
        if (!unit) return JassValue::Bool(false);
        CUnitType* ut = unit->GetUnitType();
        const int idType = args[1].intVal;
        switch (idType) {
            case 0: return JassValue::Bool(unit->IsHero());
            case 1: return JassValue::Bool(unit->IsDead());
            case 2: return JassValue::Bool(ut && ut->IsBuilding());
            case 3: return JassValue::Bool(ut && ut->GetMovementType() == PathingGridMovementType::FLY);
            case 4: return JassValue::Bool(ut && ut->GetMovementType() != PathingGridMovementType::FLY);
            case 5: return JassValue::Bool(ut && ut->GetClassifications().count(CUnitClassification::GIANT));
            case 6: return JassValue::Bool(ut && ut->GetClassifications().count(CUnitClassification::UNDEAD));
            case 7: return JassValue::Bool(ut && ut->GetClassifications().count(CUnitClassification::SUMMONED));
            case 8: return JassValue::Bool(ut && ut->GetClassifications().count(CUnitClassification::MECHANICAL));
            case 9: return JassValue::Bool(ut && ut->GetClassifications().count(CUnitClassification::PEON));
            case 10: return JassValue::Bool(ut && ut->GetClassifications().count(CUnitClassification::SAPPER));
            case 11: return JassValue::Bool(ut && ut->GetClassifications().count(CUnitClassification::TOWNHALL));
            case 12: return JassValue::Bool(ut && ut->GetClassifications().count(CUnitClassification::ANCIENT));
            case 13: return JassValue::Bool(ut && ut->GetClassifications().count(CUnitClassification::TREE));
            case 14: return JassValue::Bool(ut && ut->GetClassifications().count(CUnitClassification::WARD));
            default: return JassValue::Bool(false);
        }
    });
    vm_.RegisterNative("IsUnitType", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Bool(false);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId < 0) return JassValue::Bool(false);
        CUnit* unit = sim->GetUnit(simId);
        if (!unit) return JassValue::Bool(false);
        int typeCode = args[1].intVal;
        CUnitType* ut = unit->GetUnitType();
        switch (typeCode) {
            case 0: return JassValue::Bool(unit->IsHero());
            case 1: return JassValue::Bool(unit->IsDead());
            case 2: return JassValue::Bool(ut && ut->IsBuilding());
            case 3: return JassValue::Bool(ut && ut->GetMovementType() == PathingGridMovementType::FLY);
            case 4: return JassValue::Bool(ut && ut->GetMovementType() != PathingGridMovementType::FLY);
            case 5: {
                if (!ut) return JassValue::Bool(false);
                auto& attacks = ut->GetAttacks();
                for (auto& a : attacks) { if (a.CanTargetAir()) return JassValue::Bool(true); }
                return JassValue::Bool(false);
            }
            case 6: {
                if (!ut) return JassValue::Bool(false);
                auto& attacks = ut->GetAttacks();
                for (auto& a : attacks) { if (a.CanTargetGround()) return JassValue::Bool(true); }
                return JassValue::Bool(false);
            }
            case 7: {
                if (!ut) return JassValue::Bool(false);
                auto& attacks = ut->GetAttacks();
                for (auto& a : attacks) { if (a.GetRange() < 200.0f) return JassValue::Bool(true); }
                return JassValue::Bool(false);
            }
            case 8: {
                if (!ut) return JassValue::Bool(false);
                auto& attacks = ut->GetAttacks();
                for (auto& a : attacks) { if (a.GetRange() >= 200.0f) return JassValue::Bool(true); }
                return JassValue::Bool(false);
            }
            case 9: return JassValue::Bool(ut && ut->GetClassifications().count(CUnitClassification::GIANT));
            case 10: return JassValue::Bool(ut && ut->GetClassifications().count(CUnitClassification::SUMMONED));
            case 14: return JassValue::Bool(ut && ut->GetClassifications().count(CUnitClassification::UNDEAD));
            case 15: return JassValue::Bool(ut && ut->GetClassifications().count(CUnitClassification::MECHANICAL));
            case 16: return JassValue::Bool(ut && ut->GetClassifications().count(CUnitClassification::PEON));
            case 17: return JassValue::Bool(ut && ut->GetClassifications().count(CUnitClassification::SAPPER));
            case 18: return JassValue::Bool(ut && ut->GetClassifications().count(CUnitClassification::TOWNHALL));
            case 19: return JassValue::Bool(ut && ut->GetClassifications().count(CUnitClassification::ANCIENT));
            default: return JassValue::Bool(false);
        }
    });
    vm_.RegisterNative("GetUnitAbilityLevel", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2) return JassValue::Int(0); int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT); if (simId < 0) return JassValue::Int(0); CUnit* unit = sim->GetUnit(simId); if (!unit) return JassValue::Int(0); War3ID abilId((int)args[1].intVal); CAbility* ab = unit->GetAbility(abilId); return JassValue::Int(ab ? ab->GetLevel() : 0); });
    vm_.RegisterNative("SetUnitAbilityLevel", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3) return JassValue::Int(0);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId < 0) return JassValue::Int(args[2].intVal);
        CUnit* unit = sim->GetUnit(simId);
        if (!unit) return JassValue::Int(args[2].intVal);
        War3ID abilId((int)args[1].intVal);
        CAbility* ab = unit->GetAbility(abilId);
        if (ab) { ab->SetLevel(args[2].intVal); return JassValue::Int(args[2].intVal); }
        return JassValue::Int(0);
    });
    vm_.RegisterNative("GetUnitAcquireRange", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Real(0);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId < 0) return JassValue::Real(0);
        CUnit* unit = sim->GetUnit(simId);
        if (!unit) return JassValue::Real(0);
        if (unit->GetAcquireRange() >= 0.0f) return JassValue::Real(unit->GetAcquireRange());
        CUnitType* ut = unit->GetUnitType();
        return JassValue::Real(ut ? ut->GetDefaultAcquisitionRange() : 0.0f);
    });
    vm_.RegisterNative("GetUnitDefaultAcquireRange", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.empty()) return JassValue::Real(0); int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT); if (simId < 0) return JassValue::Real(0); CUnit* unit = sim->GetUnit(simId); if (!unit) return JassValue::Real(0); CUnitType* ut = unit->GetUnitType(); return JassValue::Real(ut ? ut->GetDefaultAcquisitionRange() : 0.0f); });
    vm_.RegisterNative("SetUnitAcquireRange", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2) return JassValue::Null(); int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT); if (simId >= 0 && sim) { CUnit* unit = sim->GetUnit(simId); if (unit) unit->SetAcquireRange(args[1].realVal); } return JassValue::Null(); });
    vm_.RegisterNative("SetUnitExploded", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.empty()) return JassValue::Null(); int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT); if (simId >= 0) { CUnit* unit = sim->GetUnit(simId); if (unit) unit->SetLife(*sim, 0.0f); } return JassValue::Null(); });
    vm_.RegisterNative("SetUnitInvulnerable", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2) return JassValue::Null(); int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT); if (simId >= 0) { CUnit* unit = sim->GetUnit(simId); if (unit) unit->SetInvulnerable(args[1].boolVal); } return JassValue::Null(); });
    vm_.RegisterNative("GetUnitMoveSpeed", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.empty()) return JassValue::Real(0); int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT); if (simId >= 0) { CUnit* unit = sim->GetUnit(simId); if (unit) return JassValue::Real((float)unit->GetSpeed()); } return JassValue::Real(0); });
    vm_.RegisterNative("SetUnitMoveSpeed", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2) return JassValue::Null(); int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT); if (simId >= 0) { CUnit* unit = sim->GetUnit(simId); if (unit) unit->SetSpeed((int)args[1].realVal); } return JassValue::Null(); });
    vm_.RegisterNative("GetUnitDefaultMoveSpeed", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.empty()) return JassValue::Real(0); int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT); if (simId < 0) return JassValue::Real(0); CUnit* unit = sim->GetUnit(simId); if (!unit) return JassValue::Real(0); CUnitType* ut = unit->GetUnitType(); return JassValue::Real(ut ? (float)ut->GetSpeed() : 0.0f); });
    vm_.RegisterNative("GetUnitFlyHeight", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.empty()) return JassValue::Real(0); int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT); if (simId >= 0) { CUnit* unit = sim->GetUnit(simId); if (unit) return JassValue::Real(unit->GetFlyHeight()); } return JassValue::Real(0); });
    vm_.RegisterNative("SetUnitFlyHeight", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2) return JassValue::Null(); int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT); if (simId >= 0) { CUnit* unit = sim->GetUnit(simId); if (unit) unit->SetFlyingHeight(args[1].realVal); } return JassValue::Null(); });
    vm_.RegisterNative("GetUnitTurnSpeed", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.empty()) return JassValue::Real(0); int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT); if (simId < 0) return JassValue::Real(0); CUnit* unit = sim->GetUnit(simId); if (!unit) return JassValue::Real(0); return JassValue::Real(unit->GetTurnRate()); });
    vm_.RegisterNative("SetUnitTurnSpeed", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2) return JassValue::Null(); int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT); if (simId >= 0 && sim) { CUnit* unit = sim->GetUnit(simId); if (unit) unit->SetTurnRate(args[1].realVal); } return JassValue::Null(); });
    vm_.RegisterNative("GetUnitPropWindow", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.empty()) return JassValue::Real(0); int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT); if (simId < 0) return JassValue::Real(0); CUnit* unit = sim->GetUnit(simId); if (!unit) return JassValue::Real(0); return JassValue::Real(unit->GetEffectivePropWindow()); });
    vm_.RegisterNative("SetUnitPropWindow", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2) return JassValue::Null(); int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT); if (simId >= 0 && sim) { CUnit* unit = sim->GetUnit(simId); if (unit) unit->SetPropWindow(args[1].realVal); } return JassValue::Null(); });
    vm_.RegisterNative("GetUnitFoodUsed", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.empty()) return JassValue::Int(0); int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT); if (simId >= 0) { CUnit* unit = sim->GetUnit(simId); if (unit) return JassValue::Int(unit->GetFoodUsed()); } return JassValue::Int(0); });
    vm_.RegisterNative("GetUnitFoodMade", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.empty()) return JassValue::Int(0); int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT); if (simId >= 0) { CUnit* unit = sim->GetUnit(simId); if (unit) return JassValue::Int(unit->GetFoodMade()); } return JassValue::Int(0); });
    vm_.RegisterNative("GetUnitName", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.empty()) return JassValue::Str(""); int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT); if (simId >= 0) { CUnit* unit = sim->GetUnit(simId); if (unit && unit->GetUnitType()) return JassValue::Str(unit->GetUnitType()->GetName()); } return JassValue::Str(""); });
    vm_.RegisterNative("GetUnitPointValue", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.empty()) return JassValue::Int(0); int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT); if (simId < 0) return JassValue::Int(0); CUnit* unit = sim->GetUnit(simId); if (!unit) return JassValue::Int(0); CUnitType* ut = unit->GetUnitType(); return JassValue::Int(ut ? ut->GetPointValue() : 0); });
    vm_.RegisterNative("GetUnitRallyPoint", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty() || !data) return JassValue::Int(0);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId < 0 || !sim) return JassValue::Int(0);
        CUnit* unit = sim->GetUnit(simId);
        if (!unit) return JassValue::Int(0);
        auto loc = std::make_shared<JassLocation>();
        loc->x = unit->GetRallyX();
        loc->y = unit->GetRallyY();
        int id = data->nextLocId++;
        data->locations[id] = loc;
        return JassValue::Int(data->AllocHandle(JassHandleInfo::LOCATION, id));
    });
    vm_.RegisterNative("SetUnitRallyPoint", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        int locId = data->GetSimHandle(args[1].intVal, JassHandleInfo::LOCATION);
        if (simId < 0 || locId < 0 || !sim) return JassValue::Null();
        CUnit* unit = sim->GetUnit(simId);
        auto it = data->locations.find(locId);
        if (unit && it != data->locations.end() && it->second) { unit->SetRallyPoint(it->second->x, it->second->y); }
        return JassValue::Null();
    });
}
void JassNatives::RegisterWidgetNatives() {
    vm_.RegisterNative("GetWidgetLife", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.empty()) return JassValue::Real(0); int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT); if (simId >= 0) { CUnit* unit = sim->GetUnit(simId); if (unit) return JassValue::Real(unit->GetLife()); } return JassValue::Real(0); });
    vm_.RegisterNative("SetWidgetLife", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2) return JassValue::Null(); int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT); if (simId >= 0) { CUnit* unit = sim->GetUnit(simId); if (unit) unit->SetLife(*sim, args[1].realVal); } return JassValue::Null(); });
    vm_.RegisterNative("GetWidgetX", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.empty()) return JassValue::Real(0); int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT); if (simId >= 0) { CUnit* unit = sim->GetUnit(simId); if (unit) return JassValue::Real(unit->GetX()); } return JassValue::Real(0); });
    vm_.RegisterNative("GetWidgetY", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.empty()) return JassValue::Real(0); int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT); if (simId >= 0) { CUnit* unit = sim->GetUnit(simId); if (unit) return JassValue::Real(unit->GetY()); } return JassValue::Real(0); });
}
void JassNatives::RegisterPlayerNatives() {
    vm_.RegisterNative("Player", [data = data_](const std::vector<JassValue>& args) -> JassValue { if (args.empty()) return JassValue::Null(); return JassValue::Int(data->AllocHandle(JassHandleInfo::PLAYER, args[0].intVal)); });
    vm_.RegisterNative("GetLocalPlayer", [data = data_](const std::vector<JassValue>&) -> JassValue { const int index = data ? data->localPlayerIndex : 0; return JassValue::Int(data ? data->AllocHandle(JassHandleInfo::PLAYER, index) : 0); });
    vm_.RegisterNative("GetOwningPlayer", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.empty()) return JassValue::Null(); int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT); if (simId >= 0) { CUnit* unit = sim->GetUnit(simId); if (unit) return JassValue::Int(data->AllocHandle(JassHandleInfo::PLAYER, unit->GetPlayerIndex())); } return JassValue::Null(); });
    vm_.RegisterNative("GetPlayerId", [data = data_](const std::vector<JassValue>& args) -> JassValue { if (args.empty()) return JassValue::Int(-1); return JassValue::Int(data->GetSimHandle(args[0].intVal, JassHandleInfo::PLAYER)); });
    vm_.RegisterNative("GetPlayerName", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.empty()) return JassValue::Str("Player"); if (sim) { if (CPlayer* p = sim->GetPlayer(ResolvePlayerId(data, args[0].intVal))) { return JassValue::Str(p->GetName()); } } return JassValue::Str("Player"); });
    vm_.RegisterNative("SetPlayerName", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2) return JassValue::Null(); if (sim) { if (CPlayer* p = sim->GetPlayer(ResolvePlayerId(data, args[0].intVal))) { p->SetName(args[1].strVal); } } return JassValue::Null(); });
    vm_.RegisterNative("GetPlayerController", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.empty()) return JassValue::Int(0); if (sim) { if (CPlayer* p = sim->GetPlayer(ResolvePlayerId(data, args[0].intVal))) { return JassValue::Int((int)p->GetController()); } } return JassValue::Int(0); });
    vm_.RegisterNative("GetPlayerSlotState", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Int(0);
        const int playerId = ResolvePlayerId(data, args[0].intVal);
        if (playerId >= 0 && playerId < (int)data->playerMapConfig.size() &&
            !data->playerMapConfig[playerId].slotActive) {
            return JassValue::Int(0);
        }
        if (sim && sim->GetPlayer(playerId)) return JassValue::Int(1);
        if (playerId >= 0 && playerId < (int)data->playerMapConfig.size() &&
            data->playerMapConfig[playerId].slotActive) {
            return JassValue::Int(1);
        }
        return JassValue::Int(0);
    });
    vm_.RegisterNative("GetPlayerState", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2 || !sim) return JassValue::Int(0); int playerId = ResolvePlayerId(data, args[0].intVal); return JassValue::Int((int)GetPlayerStateValue(*sim, playerId, args[1].intVal)); });
    vm_.RegisterNative("SetPlayerState", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3 || !sim) return JassValue::Null();
        auto* p = sim->GetPlayer(ResolvePlayerId(data, args[0].intVal));
        if (!p) return JassValue::Null();
        switch (args[1].intVal) {
            case 1: p->SetGold(args[2].intVal); break;
            case 2: p->SetLumber(args[2].intVal); break;
            case 3: p->SetHeroTokens(args[2].intVal); break;
            case 4: p->SetFoodCap(args[2].intVal); break;
            case 6: p->SetAlliedVictory(args[2].intVal != 0); break;
            case 7: p->SetGivesBounty(args[2].intVal != 0); break;
            case 11: p->SetObserver(args[2].intVal != 0); break;
            case 13: p->SetGoldUpkeepRate(args[2].intVal); break;
            case 14: p->SetLumberUpkeepRate(args[2].intVal); break;
            case 25: p->SetGameResultCode(args[2].intVal); break;
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("IsPlayerAlly", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2 || !sim) return JassValue::Bool(false); auto* p1 = sim->GetPlayer(ResolvePlayerId(data, args[0].intVal)); auto* p2 = sim->GetPlayer(ResolvePlayerId(data, args[1].intVal)); if (p1 && p2) return JassValue::Bool(p1->HasAlliance(*p2, CAllianceType::ALLIED)); return JassValue::Bool(false); });
    vm_.RegisterNative("IsPlayerEnemy", [data = data_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2) return JassValue::Bool(false); return JassValue::Bool(ResolvePlayerId(data, args[0].intVal) != ResolvePlayerId(data, args[1].intVal)); });
    vm_.RegisterNative("IsPlayerInForce", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Bool(false);
        int playerId = ResolvePlayerId(data, args[0].intVal);
        int forceId = data->GetSimHandle(args[1].intVal, JassHandleInfo::FORCE);
        if (playerId < 0 || forceId < 0) return JassValue::Bool(false);
        auto fit = data->forces.find(forceId);
        if (fit == data->forces.end()) return JassValue::Bool(false);
        return JassValue::Bool(std::find(fit->second->playerIds.begin(), fit->second->playerIds.end(), playerId) != fit->second->playerIds.end());
    });
    vm_.RegisterNative("IsPlayerObserver", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.empty()) return JassValue::Bool(false); if (sim) { if (CPlayer* p = sim->GetPlayer(ResolvePlayerId(data, args[0].intVal))) { return JassValue::Bool(p->IsObserver()); } } return JassValue::Bool(false); });
    vm_.RegisterNative("IsPlayerRacePrefSet", [data = data_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2 || !data) return JassValue::Bool(false); const int p = ResolvePlayerId(data, args[0].intVal); if (p < 0 || p >= 24) return JassValue::Bool(false); return JassValue::Bool(data->playerMapConfig[p].racePreference == args[1].intVal); });
    vm_.RegisterNative("GetPlayerRace", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.empty()) return JassValue::Int(0); if (sim) { if (CPlayer* p = sim->GetPlayer(ResolvePlayerId(data, args[0].intVal))) { return JassValue::Int(p->GetRace()); } } return JassValue::Int(0); });
    vm_.RegisterNative("GetPlayerColor", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.empty()) return JassValue::Int(0); if (sim) { if (CPlayer* p = sim->GetPlayer(ResolvePlayerId(data, args[0].intVal))) { return JassValue::Int(p->GetColor()); } } return JassValue::Int(0); });
    vm_.RegisterNative("GetPlayerStartLocation", [data = data_](const std::vector<JassValue>& args) -> JassValue { if (args.empty()) return JassValue::Int(0); const int playerId = ResolvePlayerId(data, args[0].intVal); if (playerId >= 0 && playerId < (int)data->playerMapConfig.size()) { const int startIndex = data->playerMapConfig[playerId].startLocationIndex; return JassValue::Int(startIndex >= 0 ? startIndex : playerId); } return JassValue::Int(0); });
    vm_.RegisterNative("GetPlayerTechMaxAllowed", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2 || !sim) return JassValue::Int(-1); auto* p = sim->GetPlayer(ResolvePlayerId(data, args[0].intVal)); if (p) return JassValue::Int(p->GetTechMaxAllowed(War3ID((uint32_t)args[1].intVal))); return JassValue::Int(-1); });
    vm_.RegisterNative("SetPlayerTechMaxAllowed", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 3 || !sim) return JassValue::Null(); auto* p = sim->GetPlayer(ResolvePlayerId(data, args[0].intVal)); if (p) p->SetTechMaxAllowed(War3ID((uint32_t)args[1].intVal), args[2].intVal); return JassValue::Null(); });
    vm_.RegisterNative("SetPlayerAbilityAvailable", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3 || !sim) return JassValue::Null();
        CPlayer* player = sim->GetPlayer(ResolvePlayerId(data, args[0].intVal));
        if (!player) return JassValue::Null();
        const War3ID code((uint32_t)args[1].intVal);
        const bool available = args[2].boolVal;
        player->SetAbilityDisabled(code, !available);
        for (CUnit* unit : player->GetUnits()) {
            if (!unit) continue;
            for (CAbility* ability : unit->GetAbilities()) {
                if (ability && ability->GetAlias() == code) { ability->SetDisabled(!available, CAbilityDisableType::PLAYER); }
            }
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("SetPlayerTechResearched", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 3 || !sim) return JassValue::Null(); auto* p = sim->GetPlayer(ResolvePlayerId(data, args[0].intVal)); if (p) p->SetTechLevel(*sim, War3ID((uint32_t)args[1].intVal), args[2].intVal); return JassValue::Null(); });
    vm_.RegisterNative("AddPlayerTechResearched", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3 || !sim) return JassValue::Null();
        auto* p = sim->GetPlayer(ResolvePlayerId(data, args[0].intVal));
        if (!p) return JassValue::Null();
        const War3ID id((uint32_t)args[1].intVal);
        if (sim->GetUpgradeData() && !sim->GetUpgradeData()->HasUpgrade(id)) return JassValue::Null();
        p->SetTechLevel(*sim, id, p->GetTechLevel(id) + std::max(0, args[2].intVal));
        return JassValue::Null();
    });
    vm_.RegisterNative("GetPlayerTechResearched", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2 || !sim) return JassValue::Int(0); auto* p = sim->GetPlayer(ResolvePlayerId(data, args[0].intVal)); return JassValue::Int(p ? p->GetTechLevel(War3ID((uint32_t)args[1].intVal)) : 0); });
    vm_.RegisterNative("GetPlayerTechCount", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.empty() || !sim) return JassValue::Int(0); auto* p = sim->GetPlayer(ResolvePlayerId(data, args[0].intVal)); return JassValue::Int(p ? p->GetTechCount() : 0); });
    vm_.RegisterNative("GetPlayerUnitCount", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.empty() || !sim) return JassValue::Int(0); const int playerId = ResolvePlayerId(data, args[0].intVal); int count = 0; for (auto* unit : sim->GetUnits()) { if (unit && unit->GetPlayerIndex() == playerId) ++count; } return JassValue::Int(count); });
    vm_.RegisterNative("GetPlayerStructureCount", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.empty() || !sim) return JassValue::Int(0); const int playerId = ResolvePlayerId(data, args[0].intVal); int count = 0; for (auto* unit : sim->GetUnits()) { if (unit && unit->GetPlayerIndex() == playerId && unit->IsBuilding()) ++count; } return JassValue::Int(count); });
    vm_.RegisterNative("SetPlayerAlliance", [this, data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 4 || !sim) return JassValue::Null();
        const int sourcePlayerId = ResolvePlayerId(data, args[0].intVal);
        const int targetPlayerId = ResolvePlayerId(data, args[1].intVal);
        auto* src = sim->GetPlayer(sourcePlayerId);
        if (src && targetPlayerId >= 0) {
            src->SetAlliance(targetPlayerId, static_cast<CAllianceType>(args[2].intVal & 0xF), args[3].boolVal);
            FireAllianceChange(sourcePlayerId, targetPlayerId);
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("GetPlayerHandicap", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.empty()) return JassValue::Real(1.0f); if (sim) { if (CPlayer* p = sim->GetPlayer(ResolvePlayerId(data, args[0].intVal))) { return JassValue::Real(p->GetHandicap()); } } return JassValue::Real(1.0f); });
    vm_.RegisterNative("SetPlayerHandicap", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2) return JassValue::Null(); if (sim) { if (CPlayer* p = sim->GetPlayer(ResolvePlayerId(data, args[0].intVal))) { p->SetHandicap(args[1].realVal); } } return JassValue::Null(); });
    vm_.RegisterNative("GetPlayerHandicapXP", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.empty()) return JassValue::Real(1.0f); if (sim) { if (CPlayer* p = sim->GetPlayer(ResolvePlayerId(data, args[0].intVal))) { return JassValue::Real(p->GetHandicapXP()); } } return JassValue::Real(1.0f); });
    vm_.RegisterNative("SetPlayerHandicapXP", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2) return JassValue::Null(); if (sim) { if (CPlayer* p = sim->GetPlayer(ResolvePlayerId(data, args[0].intVal))) { p->SetHandicapXP(args[1].realVal); } } return JassValue::Null(); });
    vm_.RegisterNative("SetAllyColorFilterState", [data = data_](const std::vector<JassValue>& args) -> JassValue { if (!args.empty() && data) data->allyColorFilter = args[0].intVal != 0; return JassValue::Null(); });
}
void JassNatives::RegisterHeroNatives() {
    vm_.RegisterNative("GetHeroLevel", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.empty()) return JassValue::Int(0); int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT); if (CAbility* hero = FindHeroAbility(sim, simId)) { return JassValue::Int(hero->GetHeroLevel()); } if (sim) { CUnit* unit = sim->GetUnit(simId); if (unit) return JassValue::Int(unit->GetHeroLevel()); } return JassValue::Int(0); });
    vm_.RegisterNative("SetHeroLevel", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !sim) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (simId < 0) return JassValue::Null();
        if (CAbility* hero = FindHeroAbility(sim, simId)) { hero->SetHeroLevel(*sim, sim->GetUnit(simId), args[1].intVal); } else if (CUnit* unit = sim->GetUnit(simId)) { unit->SetHeroLevel(args[1].intVal); }
        return JassValue::Null();
    });
    vm_.RegisterNative("ReviveHeroLoc", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (!sim || args.size() < 2) return JassValue::Int(0);
        const int unitId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        const int locId = data->GetSimHandle(args[1].intVal, JassHandleInfo::LOCATION);
        if (unitId < 0) return JassValue::Int(0);
        CUnit* unit = sim->GetUnit(unitId);
        if (!unit) return JassValue::Int(0);
        auto locIt = data->locations.find(locId);
        if (locIt != data->locations.end() && locIt->second) { unit->SetPosition(locIt->second->x, locIt->second->y); }
        unit->SetLife(*sim, std::max(1.0f, unit->GetMaxLife() * 0.5f));
        return JassValue::Int(args[0].intVal);
    });
    vm_.RegisterNative("GetHeroXP", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.empty()) return JassValue::Int(0); if (CAbility* hero = FindHeroAbility(sim, data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT))) { return JassValue::Int(hero->GetXP()); } return JassValue::Int(0); });
    vm_.RegisterNative("SetHeroXP", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 3 || !sim) return JassValue::Null(); if (CAbility* hero = FindHeroAbility(sim, data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT))) { hero->SetXP(*sim, sim->GetUnit(data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT)), args[1].intVal); } return JassValue::Null(); });
    vm_.RegisterNative("AddHeroXP", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 3 || !sim) return JassValue::Null(); int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT); if (CAbility* hero = FindHeroAbility(sim, simId)) { hero->AddXP(*sim, sim->GetUnit(simId), args[1].intVal); } return JassValue::Null(); });
    vm_.RegisterNative("GetHeroStr", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Int(0);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (CAbility* hero = FindHeroAbility(sim, simId)) { return JassValue::Int(hero->GetStrength().GetCurrent(hero->GetHeroLevel())); }
        if (sim) { CUnit* unit = sim->GetUnit(simId); if (unit && unit->GetUnitType()) return JassValue::Int(unit->GetUnitType()->GetStartingStrength()); }
        return JassValue::Int(0);
    });
    vm_.RegisterNative("GetHeroAgi", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Int(0);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (CAbility* hero = FindHeroAbility(sim, simId)) { return JassValue::Int(hero->GetAgility().GetCurrent(hero->GetHeroLevel())); }
        if (sim) { CUnit* unit = sim->GetUnit(simId); if (unit && unit->GetUnitType()) return JassValue::Int(unit->GetUnitType()->GetStartingAgility()); }
        return JassValue::Int(0);
    });
    vm_.RegisterNative("GetHeroInt", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Int(0);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (CAbility* hero = FindHeroAbility(sim, simId)) { return JassValue::Int(hero->GetIntelligence().GetCurrent(hero->GetHeroLevel())); }
        if (sim) { CUnit* unit = sim->GetUnit(simId); if (unit && unit->GetUnitType()) return JassValue::Int(unit->GetUnitType()->GetStartingIntelligence()); }
        return JassValue::Int(0);
    });
    vm_.RegisterNative("SetHeroStr", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 3 || !sim) return JassValue::Null(); int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT); if (CAbility* hero = FindHeroAbility(sim, simId)) { hero->SetStrengthBase(args[1].intVal); hero->AddStrengthBonus(*sim, sim->GetUnit(simId), 0); } return JassValue::Null(); });
    vm_.RegisterNative("SetHeroAgi", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 3 || !sim) return JassValue::Null(); int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT); if (CAbility* hero = FindHeroAbility(sim, simId)) { hero->SetAgilityBase(args[1].intVal); hero->AddAgilityBonus(*sim, sim->GetUnit(simId), 0); } return JassValue::Null(); });
    vm_.RegisterNative("SetHeroInt", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 3 || !sim) return JassValue::Null(); int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT); if (CAbility* hero = FindHeroAbility(sim, simId)) { hero->SetIntelligenceBase(args[1].intVal); hero->AddIntelligenceBonus(*sim, sim->GetUnit(simId), 0); } return JassValue::Null(); });
    vm_.RegisterNative("ModifyHeroStat", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 4 || !sim) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        CAbility* hero = FindHeroAbility(sim, simId);
        if (!hero) return JassValue::Null();
        CUnit* unit = sim->GetUnit(simId);
        const int stat = args[1].intVal;
        const int method = args[2].intVal;
        const int value = args[3].intVal;
        if (stat == 0) {
            if (method == 0) { hero->SetStrengthBase(value); hero->AddStrengthBonus(*sim, unit, 0); }
            if (method == 1) hero->AddStrengthBonus(*sim, unit, value);
            if (method == 2) hero->AddStrengthBonus(*sim, unit, -value);
        } else if (stat == 1) {
            if (method == 0) { hero->SetAgilityBase(value); hero->AddAgilityBonus(*sim, unit, 0); }
            if (method == 1) hero->AddAgilityBonus(*sim, unit, value);
            if (method == 2) hero->AddAgilityBonus(*sim, unit, -value);
        } else if (stat == 2) {
            if (method == 0) { hero->SetIntelligenceBase(value); hero->AddIntelligenceBonus(*sim, unit, 0); }
            if (method == 1) hero->AddIntelligenceBonus(*sim, unit, value);
            if (method == 2) hero->AddIntelligenceBonus(*sim, unit, -value);
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("UnitAddHeroAbility", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Bool(true); });
    vm_.RegisterNative("SelectHeroSkill", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2 || !sim) return JassValue::Null(); int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT); if (CAbility* hero = FindHeroAbility(sim, simId)) { hero->SelectHeroSkill(*sim, sim->GetUnit(simId), War3ID((uint32_t)args[1].intVal)); } return JassValue::Null(); });
    vm_.RegisterNative("GetHeroSkillPoints", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.empty()) return JassValue::Int(0); if (CAbility* hero = FindHeroAbility(sim, data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT))) { return JassValue::Int(hero->GetSkillPoints()); } return JassValue::Int(0); });
    vm_.RegisterNative("UnitModifySkillPoints", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2) return JassValue::Bool(false); CAbility* hero = FindHeroAbility(sim, data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT)); if (!hero) return JassValue::Bool(false); hero->SetSkillPoints(std::max(0, hero->GetSkillPoints() + args[1].intVal)); return JassValue::Bool(true); });
    vm_.RegisterNative("UnitStripHeroLevel", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2 || !sim) return JassValue::Bool(false); int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT); if (CAbility* hero = FindHeroAbility(sim, simId)) { return JassValue::Bool(hero->StripLevels(*sim, sim->GetUnit(simId), args[1].intVal)); } return JassValue::Bool(false); });
    vm_.RegisterNative("UnitRemoveHeroAbility", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Bool(false); });
}
void JassNatives::RegisterItemNatives() {
    vm_.RegisterNative("CreateItem", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 3 || !sim) return JassValue::Int(0); War3ID w3id((uint32_t)args[0].intVal); float x = args[1].realVal, y = args[2].realVal; return JassValue::Int(data ? data->CreateItemHandle(sim, w3id, x, y) : 0); });
    vm_.RegisterNative("BlzCreateItemWithSkin", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 3 || !sim) return JassValue::Int(0); War3ID w3id((uint32_t)args[0].intVal); float x = args[1].realVal, y = args[2].realVal; return JassValue::Int(data ? data->CreateItemHandle(sim, w3id, x, y) : 0); });
    vm_.RegisterNative("ChooseRandomItem", [sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.empty() || !sim || !sim->GetItemData()) return JassValue::Int(0); const War3ID itemId = sim->GetItemData()->ChooseRandomItem( args[0].intVal, [sim]() { return sim->SimRand01(); }); return JassValue::Int((int)itemId.value); });
    vm_.RegisterNative("ChooseRandomItemEx", [sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2 || !sim || !sim->GetItemData()) return JassValue::Int(0); const CItemClass itemClass = ItemClassFromJassOrdinal(args[0].intVal); const War3ID itemId = sim->GetItemData()->ChooseRandomItem( itemClass, args[1].intVal, [sim]() { return sim->SimRand01(); }); return JassValue::Int((int)itemId.value); });
    vm_.RegisterNative("CreateItemLoc", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !sim) return JassValue::Int(0);
        int locId = data->GetSimHandle(args[1].intVal, JassHandleInfo::LOCATION);
        if (locId < 0) return JassValue::Int(0);
        auto locIt = data->locations.find(locId);
        if (locIt == data->locations.end()) return JassValue::Int(0);
        War3ID w3id((uint32_t)args[0].intVal);
        return JassValue::Int(data ? data->CreateItemHandle(sim, w3id, locIt->second->x, locIt->second->y) : 0);
    });
    vm_.RegisterNative("RemoveItem", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.empty()) return JassValue::Null(); const int itemId = data ? data->GetSimHandle(args[0].intVal, JassHandleInfo::ITEM) : -1; if (sim && itemId >= 0) { if (auto* item = GetJassItem(data, args[0].intVal)) { item->SetLife(*sim, 0.0f); sim->RemoveItem(item); } } if (itemId >= 0) { data->items.erase(itemId); data->FreeHandle(args[0].intVal); } return JassValue::Null(); });
    vm_.RegisterNative("GetItemTypeId", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.empty()) return JassValue::Int(0); (void)sim; auto* item = GetJassItem(data, args[0].intVal); return JassValue::Int(item ? (int)item->GetTypeId().value : 0); });
    vm_.RegisterNative("GetItemLife", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.empty()) return JassValue::Real(100); (void)sim; auto* item = GetJassItem(data, args[0].intVal); return JassValue::Real(item ? item->GetLife() : 100.0f); });
    vm_.RegisterNative("SetItemLife", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2 || !sim) return JassValue::Null(); if (auto* item = GetJassItem(data, args[0].intVal)) { item->SetLife(*sim, args[1].realVal); } return JassValue::Null(); });
    vm_.RegisterNative("GetItemX", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.empty()) return JassValue::Real(0); (void)sim; auto* item = GetJassItem(data, args[0].intVal); return JassValue::Real(item ? item->GetX() : 0.0f); });
    vm_.RegisterNative("GetItemY", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.empty()) return JassValue::Real(0); (void)sim; auto* item = GetJassItem(data, args[0].intVal); return JassValue::Real(item ? item->GetY() : 0.0f); });
    vm_.RegisterNative("SetItemPosition", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 3) return JassValue::Null(); (void)sim; if (auto* item = GetJassItem(data, args[0].intVal)) { item->SetPosition(args[1].realVal, args[2].realVal); } return JassValue::Null(); });
    vm_.RegisterNative("SetItemDropOnDeath", [data = data_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2) return JassValue::Null(); if (auto* item = GetJassItem(data, args[0].intVal)) { item->SetDroppedOnDeath(args[1].boolVal); } return JassValue::Null(); });
    vm_.RegisterNative("SetItemDroppable", [data = data_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2) return JassValue::Null(); if (auto* item = GetJassItem(data, args[0].intVal)) { item->SetDroppable(args[1].boolVal); } return JassValue::Null(); });
    vm_.RegisterNative("SetItemPawnable", [data = data_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2) return JassValue::Null(); if (auto* item = GetJassItem(data, args[0].intVal)) { item->SetPawnable(args[1].boolVal); } return JassValue::Null(); });
    vm_.RegisterNative("SetItemInvulnerable", [data = data_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2) return JassValue::Null(); if (auto* item = GetJassItem(data, args[0].intVal)) { item->SetInvulnerable(args[1].boolVal); } return JassValue::Null(); });
    vm_.RegisterNative("IsItemInvulnerable", [data = data_](const std::vector<JassValue>& args) -> JassValue { if (args.empty()) return JassValue::Bool(false); auto* item = GetJassItem(data, args[0].intVal); return JassValue::Bool(item ? item->IsInvulnerable() : false); });
    vm_.RegisterNative("SetItemVisible", [data = data_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2) return JassValue::Null(); if (auto* item = GetJassItem(data, args[0].intVal)) { item->SetHidden(!args[1].boolVal); } return JassValue::Null(); });
    vm_.RegisterNative("IsItemVisible", [data = data_](const std::vector<JassValue>& args) -> JassValue { if (args.empty()) return JassValue::Bool(false); auto* item = GetJassItem(data, args[0].intVal); return JassValue::Bool(item ? !item->IsHidden() : false); });
    vm_.RegisterNative("IsItemPowerup", [data = data_](const std::vector<JassValue>& args) -> JassValue { if (args.empty()) return JassValue::Bool(false); auto* item = GetJassItem(data, args[0].intVal); return JassValue::Bool(item ? item->IsPowerup() : false); });
    vm_.RegisterNative("IsItemOwned", [data = data_](const std::vector<JassValue>& args) -> JassValue { if (args.empty()) return JassValue::Bool(false); auto* item = GetJassItem(data, args[0].intVal); return JassValue::Bool(item ? item->GetOwner() != nullptr : false); });
    vm_.RegisterNative("IsItemSellable", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.empty() || !sim || !sim->GetItemData()) return JassValue::Bool(false); auto* item = GetJassItem(data, args[0].intVal); const CItemType* type = item ? sim->GetItemData()->GetItem(item->GetTypeId()) : nullptr; return JassValue::Bool(type ? type->sellable : false); });
    vm_.RegisterNative("IsItemPawnable", [data = data_](const std::vector<JassValue>& args) -> JassValue { if (args.empty()) return JassValue::Bool(false); auto* item = GetJassItem(data, args[0].intVal); return JassValue::Bool(item ? item->IsPawnable() : false); });
    vm_.RegisterNative("IsItemIdPowerup", [sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.empty() || !sim || !sim->GetItemData()) return JassValue::Bool(false); const CItemType* type = sim->GetItemData()->GetItem(War3ID((uint32_t)args[0].intVal)); return JassValue::Bool(type ? type->powerup : false); });
    vm_.RegisterNative("IsItemIdSellable", [sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.empty() || !sim || !sim->GetItemData()) return JassValue::Bool(false); const CItemType* type = sim->GetItemData()->GetItem(War3ID((uint32_t)args[0].intVal)); return JassValue::Bool(type ? type->sellable : false); });
    vm_.RegisterNative("IsItemIdPawnable", [sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.empty() || !sim || !sim->GetItemData()) return JassValue::Bool(false); const CItemType* type = sim->GetItemData()->GetItem(War3ID((uint32_t)args[0].intVal)); return JassValue::Bool(type ? type->pawnable : false); });
    vm_.RegisterNative("IsItemIdPerishable", [sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.empty() || !sim || !sim->GetItemData()) return JassValue::Bool(false); const CItemType* type = sim->GetItemData()->GetItem(War3ID((uint32_t)args[0].intVal)); return JassValue::Bool(type ? type->perishable : false); });
    vm_.RegisterNative("EnumItemsInRect", [data = data_, sim = sim_, vmPtr = vmPtr_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Null();
        const int rectId = data->GetSimHandle(args[0].intVal, JassHandleInfo::RECT);
        if (rectId < 0) return JassValue::Null();
        auto rectIt = data->rects.find(rectId);
        if (rectIt == data->rects.end()) return JassValue::Null();
        const std::string filterFunc = (args.size() > 1 && args[1].type == JassValue::STRING) ? args[1].strVal : "";
        const std::string actionFunc = (args.size() > 2 && args[2].type == JassValue::STRING) ? args[2].strVal : "";
        const int previousFilterItem = data->filterItemHandle;
        const int previousEnumItem = data->enumItemHandle;
        const auto rect = rectIt->second;
        std::vector<int> matchingItems;
        auto acceptItem = [&](int itemId, CItem* item) { if (!item || item->IsDead() || item->IsHidden()) return; data->items[itemId] = item; const int itemHandle = data->AllocHandle(JassHandleInfo::ITEM, itemId); bool accepted = true; if (!filterFunc.empty() && vmPtr) { data->filterItemHandle = itemHandle; accepted = JassValueToBool(CallSyncFunction(vmPtr, filterFunc)); } if (accepted) { matchingItems.push_back(itemHandle); } };
        bool usedWorldCollision = false;
        if (sim && sim->GetWorldCollision()) {
            std::vector<CItem*> worldItems;
            sim->GetWorldCollision()->EnumItemsInRect(rect->minX, rect->minY,
                                                      rect->maxX - rect->minX,
                                                      rect->maxY - rect->minY,
                                                      worldItems);
            for (CItem* item : worldItems) {
                acceptItem(item->GetHandleId(), item);
            }
            usedWorldCollision = true;
        }
        if (!usedWorldCollision) {
            for (const auto& [itemId, item] : data->items) {
                if (!item || item->IsDead() || item->IsHidden()) continue;
                const float x = item->GetX();
                const float y = item->GetY();
                if (x < rect->minX || x > rect->maxX || y < rect->minY || y > rect->maxY) continue;
                acceptItem(itemId, item);
            }
        }
        data->filterItemHandle = previousFilterItem;
        if (!actionFunc.empty() && vmPtr) {
            for (int itemHandle : matchingItems) {
                data->enumItemHandle = itemHandle;
                CallSyncFunction(vmPtr, actionFunc);
            }
        }
        data->enumItemHandle = previousEnumItem;
        return JassValue::Null();
    });
    vm_.RegisterNative("GetItemPlayer", [data = data_](const std::vector<JassValue>& args) -> JassValue { if (args.empty()) return JassValue::Null(); auto* item = GetJassItem(data, args[0].intVal); auto* owner = item ? item->GetOwner() : nullptr; return owner ? JassValue::Int(data->AllocHandle(JassHandleInfo::PLAYER, owner->GetPlayerIndex())) : JassValue::Null(); });
    vm_.RegisterNative("SetItemPlayer", [](const std::vector<JassValue>&) { return JassValue::Null(); });
    vm_.RegisterNative("GetItemCharges", [data = data_](const std::vector<JassValue>& args) -> JassValue { if (args.empty()) return JassValue::Int(0); auto* item = GetJassItem(data, args[0].intVal); return JassValue::Int(item ? item->GetCharges() : 0); });
    vm_.RegisterNative("SetItemCharges", [data = data_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2) return JassValue::Null(); if (auto* item = GetJassItem(data, args[0].intVal)) { item->SetCharges(args[1].intVal); item->SetCharged(args[1].intVal > 0); } return JassValue::Null(); });
    vm_.RegisterNative("GetItemName", [data = data_](const std::vector<JassValue>& args) -> JassValue { if (args.empty()) return JassValue::Str(""); auto* item = GetJassItem(data, args[0].intVal); return JassValue::Str(item ? item->GetName() : ""); });
    vm_.RegisterNative("GetItemLevel", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.empty() || !sim || !sim->GetItemData()) return JassValue::Int(0); auto* item = GetJassItem(data, args[0].intVal); if (!item) return JassValue::Int(0); const CItemType* type = sim->GetItemData()->GetItem(item->GetTypeId()); return JassValue::Int(type ? type->level : 0); });
    vm_.RegisterNative("GetItemType", [data = data_](const std::vector<JassValue>& args) -> JassValue { if (args.empty()) return JassValue::Int(0); auto* item = GetJassItem(data, args[0].intVal); return JassValue::Int(item ? (int)item->GetItemClass() : 0); });
    vm_.RegisterNative("SetItemDropID", [data = data_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2) return JassValue::Null(); if (auto* item = GetJassItem(data, args[0].intVal)) { item->SetDropId(War3ID((uint32_t)args[1].intVal)); } return JassValue::Null(); });
    vm_.RegisterNative("GetItemDropID", [data = data_](const std::vector<JassValue>& args) -> JassValue { if (args.empty()) return JassValue::Int(0); auto* item = GetJassItem(data, args[0].intVal); return JassValue::Int(item ? (int)item->GetDropId().value : 0); });
    vm_.RegisterNative("GetItemUserData", [data = data_](const std::vector<JassValue>& args) -> JassValue { if (args.empty()) return JassValue::Int(0); auto* item = GetJassItem(data, args[0].intVal); return JassValue::Int(item ? item->GetUserData() : 0); });
    vm_.RegisterNative("SetItemUserData", [data = data_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2) return JassValue::Null(); if (auto* item = GetJassItem(data, args[0].intVal)) { item->SetUserData(args[1].intVal); } return JassValue::Null(); });
    vm_.RegisterNative("UnitAddItem", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2 || !sim) return JassValue::Bool(false); CUnit* unit = nullptr; CAbility* inventory = FindUnitInventory(data, sim, args[0].intVal, &unit); CItem* item = GetJassItem(data, args[1].intVal); return JassValue::Bool(inventory && item && inventory->PickUpItem(*sim, unit, item)); });
    vm_.RegisterNative("UnitAddItemById", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !sim) return JassValue::Int(0);
        CUnit* unit = ResolveJassUnit(data, sim, args[0].intVal);
        if (!unit) return JassValue::Int(0);
        const int itemHandle = data ? data->CreateItemHandle(sim, War3ID((uint32_t)args[1].intVal), unit->GetX(), unit->GetY()) : 0;
        if (CItem* item = GetJassItem(data, itemHandle)) {
            if (CAbility* inventory = unit->GetAbility(War3ID("AInv"))) { inventory->PickUpItem(*sim, unit, item); }
        }
        return JassValue::Int(itemHandle);
    });
    vm_.RegisterNative("UnitAddItemToSlotById", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3 || !sim) return JassValue::Int(0);
        CUnit* unit = ResolveJassUnit(data, sim, args[0].intVal);
        if (!unit) return JassValue::Int(0);
        const int itemHandle = data ? data->CreateItemHandle(sim, War3ID((uint32_t)args[1].intVal), unit->GetX(), unit->GetY()) : 0;
        if (CItem* item = GetJassItem(data, itemHandle)) {
            if (CAbility* inventory = unit->GetAbility(War3ID("AInv"))) { inventory->PickUpItem(*sim, unit, item, args[2].intVal); }
        }
        return JassValue::Int(itemHandle);
    });
    vm_.RegisterNative("UnitRemoveItem", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2 || !sim) return JassValue::Null(); CUnit* unit = nullptr; CAbility* inventory = FindUnitInventory(data, sim, args[0].intVal, &unit); if (CItem* item = GetJassItem(data, args[1].intVal); inventory && item) { inventory->DropItem(*sim, unit, item); } return JassValue::Null(); });
    vm_.RegisterNative("UnitRemoveItemFromSlot", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !sim) return JassValue::Int(0);
        CUnit* unit = nullptr;
        CAbility* inventory = FindUnitInventory(data, sim, args[0].intVal, &unit);
        CItem* item = inventory ? inventory->GetItem(args[1].intVal) : nullptr;
        if (!item) return JassValue::Int(0);
        const int handle = data->AllocHandle(JassHandleInfo::ITEM, item->GetHandleId());
        inventory->DropItem(*sim, unit, args[1].intVal);
        return JassValue::Int(handle);
    });
    vm_.RegisterNative("UnitHasItem", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2 || !sim) return JassValue::Bool(false); CAbility* inventory = FindUnitInventory(data, sim, args[0].intVal); CItem* item = GetJassItem(data, args[1].intVal); return JassValue::Bool(inventory && item && inventory->HasItem(item)); });
    vm_.RegisterNative("UnitItemInSlot", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2 || !sim) return JassValue::Int(0); CAbility* inventory = FindUnitInventory(data, sim, args[0].intVal); CItem* item = inventory ? inventory->GetItem(args[1].intVal) : nullptr; return JassValue::Int(item ? data->AllocHandle(JassHandleInfo::ITEM, item->GetHandleId()) : 0); });
    vm_.RegisterNative("UnitInventorySize", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.empty() || !sim) return JassValue::Int(0); CAbility* inventory = FindUnitInventory(data, sim, args[0].intVal); return JassValue::Int(inventory ? inventory->GetItemCapacity() : 0); });
    vm_.RegisterNative("UnitDropItemPoint", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 4 || !sim) return JassValue::Bool(false); CUnit* unit = nullptr; CAbility* inventory = FindUnitInventory(data, sim, args[0].intVal, &unit); CItem* item = GetJassItem(data, args[1].intVal); return JassValue::Bool(inventory && item && inventory->DropItemAt(*sim, unit, item, args[2].realVal, args[3].realVal)); });
    vm_.RegisterNative("UnitDropItemSlot", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 3 || !sim) return JassValue::Bool(false); CAbility* inventory = FindUnitInventory(data, sim, args[0].intVal); CItem* item = GetJassItem(data, args[1].intVal); return JassValue::Bool(inventory && item && inventory->MoveItemToSlot(item, args[2].intVal)); });
    vm_.RegisterNative("UnitDropItemTarget", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3 || !sim) return JassValue::Bool(false);
        CUnit* unit = nullptr;
        CAbility* inventory = FindUnitInventory(data, sim, args[0].intVal, &unit);
        CItem* item = GetJassItem(data, args[1].intVal);
        CWidget* target = GetJassWidget(data, sim, args[2].intVal);
        return JassValue::Bool(inventory && item && target &&
                               inventory->DropItemAt(*sim, unit, item, target->GetX(), target->GetY()));
    });
    auto unitUseItemFn = [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2 || !sim) return JassValue::Bool(false); CUnit* unit = nullptr; CAbility* inventory = FindUnitInventory(data, sim, args[0].intVal, &unit); CItem* item = GetJassItem(data, args[1].intVal); return JassValue::Bool(inventory && item && inventory->UseItem(*sim, unit, item)); };
    vm_.RegisterNative("UnitUseItem", unitUseItemFn);
    vm_.RegisterNative("UnitUseItemPoint", [unitUseItemFn](const std::vector<JassValue>& args) -> JassValue { return args.size() < 4 ? JassValue::Bool(false) : unitUseItemFn(args); });
    vm_.RegisterNative("UnitUseItemTarget", [unitUseItemFn](const std::vector<JassValue>& args) -> JassValue { return args.size() < 3 ? JassValue::Bool(false) : unitUseItemFn(args); });
}
void JassNatives::RegisterDestructableNatives() {
    vm_.RegisterNative("CreateDestructable", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 5 || !sim) return JassValue::Int(0);
        War3ID w3id((uint32_t)args[0].intVal);
        float x = args[1].realVal, y = args[2].realVal, face = args[3].realVal, scale = args[4].realVal;
        (void)face; (void)scale;
        auto* dest = sim->CreateDestructable(w3id, x, y, 100.0f);
        if (!dest) return JassValue::Int(0);
        int jassHandle = data->AllocHandle(JassHandleInfo::DESTRUCTABLE, dest->GetHandleId());
        return JassValue::Int(jassHandle);
    });
    vm_.RegisterNative("CreateDestructableZ", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 5 || !sim) return JassValue::Int(0); War3ID w3id((uint32_t)args[0].intVal); float x = args[1].realVal, y = args[2].realVal; auto* dest = sim->CreateDestructable(w3id, x, y, 100.0f); if (!dest) return JassValue::Int(0); return JassValue::Int(data->AllocHandle(JassHandleInfo::DESTRUCTABLE, dest->GetHandleId())); });
    vm_.RegisterNative("CreateDeadDestructable", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 5 || !sim) return JassValue::Int(0); War3ID w3id((uint32_t)args[0].intVal); float x = args[1].realVal, y = args[2].realVal; auto* dest = sim->CreateDestructable(w3id, x, y, 0.0f); if (!dest) return JassValue::Int(0); return JassValue::Int(data->AllocHandle(JassHandleInfo::DESTRUCTABLE, dest->GetHandleId())); });
    vm_.RegisterNative("CreateDeadDestructableZ", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 5 || !sim) return JassValue::Int(0); War3ID w3id((uint32_t)args[0].intVal); float x = args[1].realVal, y = args[2].realVal; auto* dest = sim->CreateDestructable(w3id, x, y, 0.0f); if (!dest) return JassValue::Int(0); return JassValue::Int(data->AllocHandle(JassHandleInfo::DESTRUCTABLE, dest->GetHandleId())); });
    vm_.RegisterNative("RemoveDestructable", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.empty()) return JassValue::Null(); int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::DESTRUCTABLE); if (sim) { if (auto* destructable = sim->GetDestructable(id)) { sim->RemoveDestructable(destructable); } } if (id >= 0) data->FreeHandle(args[0].intVal); return JassValue::Null(); });
    vm_.RegisterNative("KillDestructable", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.empty() || !sim) return JassValue::Null(); const int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::DESTRUCTABLE); if (auto* destructable = sim->GetDestructable(id)) { destructable->SetLife(*sim, 0.0f); } return JassValue::Null(); });
    vm_.RegisterNative("GetDestructableTypeId", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.empty()) return JassValue::Int(0); if (!sim) return JassValue::Int(0); int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::DESTRUCTABLE); auto* destructable = sim->GetDestructable(id); return JassValue::Int(destructable ? (int)destructable->GetTypeId().value : 0); });
    vm_.RegisterNative("GetDestructableLife", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.empty()) return JassValue::Real(100); if (!sim) return JassValue::Real(100); int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::DESTRUCTABLE); auto* destructable = sim->GetDestructable(id); return JassValue::Real(destructable ? destructable->GetLife() : 100.0f); });
    vm_.RegisterNative("SetDestructableLife", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2 || !sim) return JassValue::Null(); int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::DESTRUCTABLE); if (auto* destructable = sim->GetDestructable(id)) { destructable->SetLife(*sim, args[1].realVal); } return JassValue::Null(); });
    vm_.RegisterNative("GetDestructableMaxLife", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.empty() || !sim) return JassValue::Real(100); int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::DESTRUCTABLE); auto* destructable = sim->GetDestructable(id); return JassValue::Real(destructable ? destructable->GetMaxLife() : 100.0f); });
    vm_.RegisterNative("SetDestructableMaxLife", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2 || !sim) return JassValue::Null(); int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::DESTRUCTABLE); if (auto* destructable = sim->GetDestructable(id)) { destructable->SetMaxLife(args[1].realVal); if (destructable->GetLife() > destructable->GetMaxLife()) { destructable->SetLife(*sim, destructable->GetMaxLife()); } } return JassValue::Null(); });
    vm_.RegisterNative("GetDestructableX", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.empty()) return JassValue::Real(0); if (!sim) return JassValue::Real(0); int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::DESTRUCTABLE); auto* destructable = sim->GetDestructable(id); return JassValue::Real(destructable ? destructable->GetX() : 0.0f); });
    vm_.RegisterNative("GetDestructableY", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.empty()) return JassValue::Real(0); if (!sim) return JassValue::Real(0); int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::DESTRUCTABLE); auto* destructable = sim->GetDestructable(id); return JassValue::Real(destructable ? destructable->GetY() : 0.0f); });
    vm_.RegisterNative("DestructableRestoreLife", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2 || !sim) return JassValue::Null(); int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::DESTRUCTABLE); if (id >= 0) { if (auto* destructable = sim->GetDestructable(id)) { destructable->SetLife(*sim, std::min(args[1].realVal, destructable->GetMaxLife())); } } return JassValue::Null(); });
    vm_.RegisterNative("QueueDestructableAnimation", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2 || !sim) return JassValue::Null(); int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::DESTRUCTABLE); if (id >= 0) { if (auto* destructable = sim->GetDestructable(id)) destructable->QueueAnimation(args[1].strVal); } return JassValue::Null(); });
    vm_.RegisterNative("SetDestructableAnimation", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2 || !sim) return JassValue::Null(); int id = data->GetSimHandle(args[0].intVal, JassHandleInfo::DESTRUCTABLE); if (id >= 0) { if (auto* destructable = sim->GetDestructable(id)) destructable->SetAnimation(args[1].strVal); } return JassValue::Null(); });
    vm_.RegisterNative("WaygateActivate", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2) return JassValue::Null(); int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT); if (simId >= 0 && sim) { CUnit* unit = sim->GetUnit(simId); if (unit) unit->SetWaygateActive(args[1].boolVal); } return JassValue::Null(); });
    vm_.RegisterNative("WaygateSetDestination", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 3) return JassValue::Null(); int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT); if (simId >= 0 && sim) { CUnit* unit = sim->GetUnit(simId); if (unit) unit->SetWaygateDestination(args[1].realVal, args[2].realVal); } return JassValue::Null(); });
    vm_.RegisterNative("WaygateGetDestinationX", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.empty()) return JassValue::Real(0); int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT); if (simId >= 0 && sim) { CUnit* unit = sim->GetUnit(simId); if (unit) return JassValue::Real(unit->GetWaygateX()); } return JassValue::Real(0); });
    vm_.RegisterNative("WaygateGetDestinationY", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.empty()) return JassValue::Real(0); int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT); if (simId >= 0 && sim) { CUnit* unit = sim->GetUnit(simId); if (unit) return JassValue::Real(unit->GetWaygateY()); } return JassValue::Real(0); });
    vm_.RegisterNative("EnumDestructablesInRect", [data = data_, sim = sim_, vmPtr = vmPtr_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Null();
        const int rectId = data->GetSimHandle(args[0].intVal, JassHandleInfo::RECT);
        if (rectId < 0) return JassValue::Null();
        auto rectIt = data->rects.find(rectId);
        if (rectIt == data->rects.end()) return JassValue::Null();
        const std::string filterFunc = (args.size() > 1 && args[1].type == JassValue::STRING) ? args[1].strVal : "";
        const std::string actionFunc = (args.size() > 2 && args[2].type == JassValue::STRING) ? args[2].strVal : "";
        const int previousFilterDest = data->filterDestructableHandle;
        const int previousEnumDest = data->enumDestructableHandle;
        const auto rect = rectIt->second;
        std::vector<int> matching;
        if (sim) {
            for (CDestructable* dest : sim->GetDestructables()) {
                if (!dest || dest->IsDead()) continue;
                const float x = dest->GetX();
                const float y = dest->GetY();
                if (x < rect->minX || x > rect->maxX || y < rect->minY || y > rect->maxY) continue;
                const int destHandle = data->AllocHandle(JassHandleInfo::DESTRUCTABLE,
                                                         dest->GetHandleId());
                bool accepted = true;
                if (!filterFunc.empty() && vmPtr && vmPtr->HasFunction(filterFunc)) {
                    data->filterDestructableHandle = destHandle;
                    accepted = JassValueToBool(CallSyncFunction(vmPtr, filterFunc));
                }
                if (accepted) { matching.push_back(destHandle); }
            }
        }
        data->filterDestructableHandle = previousFilterDest;
        if (!actionFunc.empty() && vmPtr && vmPtr->HasFunction(actionFunc)) {
            for (int destHandle : matching) {
                data->enumDestructableHandle = destHandle;
                CallSyncFunction(vmPtr, actionFunc);
            }
        }
        data->enumDestructableHandle = previousEnumDest;
        return JassValue::Null();
    });
}
void JassNatives::RegisterOrderIdNatives() {
    auto issueNoTarget = [data = data_, sim = sim_](int unitHandle, int orderId) -> bool { if (orderId <= 0 || !sim) return false; int uSim = data->GetSimHandle(unitHandle, JassHandleInfo::UNIT); if (uSim < 0) return false; CUnit* u = sim->GetUnit(uSim); if (!u) return false; u->IssueOrder(std::make_unique<COrderNoTarget>(orderId)); return true; };
    auto issuePoint = [data = data_, sim = sim_](int unitHandle, int orderId, float x, float y) -> bool { if (orderId <= 0 || !sim) return false; int uSim = data->GetSimHandle(unitHandle, JassHandleInfo::UNIT); if (uSim < 0) return false; CUnit* u = sim->GetUnit(uSim); if (!u) return false; u->IssueOrder(std::make_unique<COrderTargetPoint>(orderId, x, y)); return true; };
    auto issueTarget = [data = data_, sim = sim_](int unitHandle, int orderId, int targetHandle) -> bool { if (orderId <= 0 || !sim) return false; int uSim = data->GetSimHandle(unitHandle, JassHandleInfo::UNIT); if (uSim < 0) return false; CUnit* u = sim->GetUnit(uSim); if (!u) return false; CWidget* target = ResolveOrderTargetWidget(data, sim, targetHandle); if (!target) return false; u->IssueOrder(std::make_unique<COrderTargetWidget>(orderId, target)); return true; };
    auto groupUnits = [data = data_](int groupHandle) -> std::vector<int> { int simId = data->GetSimHandle(groupHandle, JassHandleInfo::GROUP); if (simId < 0) return {}; auto git = data->groups.find(simId); if (git == data->groups.end()) return {}; return git->second->unitHandles; };
    auto locationXY = [data = data_](int locHandle, float& x, float& y) -> bool { int locSim = data->GetSimHandle(locHandle, JassHandleInfo::LOCATION); if (locSim < 0) return false; auto it = data->locations.find(locSim); if (it == data->locations.end()) return false; x = it->second->x; y = it->second->y; return true; };
    vm_.RegisterNative("IssueImmediateOrderById", [issueNoTarget](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2) return JassValue::Bool(false); return JassValue::Bool(issueNoTarget(args[0].intVal, args[1].intVal)); });
    vm_.RegisterNative("IssuePointOrderById", [issuePoint](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 4) return JassValue::Bool(false); return JassValue::Bool(issuePoint(args[0].intVal, args[1].intVal, args[2].realVal, args[3].realVal)); });
    vm_.RegisterNative("IssuePointOrderByIdLoc", [issuePoint, locationXY](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 3) return JassValue::Bool(false); float x = 0, y = 0; if (!locationXY(args[2].intVal, x, y)) return JassValue::Bool(false); return JassValue::Bool(issuePoint(args[0].intVal, args[1].intVal, x, y)); });
    vm_.RegisterNative("IssueTargetOrderById", [issueTarget](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 3) return JassValue::Bool(false); return JassValue::Bool(issueTarget(args[0].intVal, args[1].intVal, args[2].intVal)); });
    vm_.RegisterNative("IssueBuildOrderById", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 4 || !sim) return JassValue::Bool(false);
        int uSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (uSim < 0) return JassValue::Bool(false);
        CUnit* u = sim->GetUnit(uSim);
        if (!u) return JassValue::Bool(false);
        u->IssueOrder(std::make_unique<COrderTargetPoint>((int)(uint32_t)args[1].intVal, args[2].realVal, args[3].realVal));
        return JassValue::Bool(true);
    });
    vm_.RegisterNative("IssueInstantPointOrder", [issuePoint](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 4) return JassValue::Bool(false); return JassValue::Bool(issuePoint(args[0].intVal, ParseOrderString(args[1].strVal), args[2].realVal, args[3].realVal)); });
    vm_.RegisterNative("IssueInstantPointOrderById", [issuePoint](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 4) return JassValue::Bool(false); return JassValue::Bool(issuePoint(args[0].intVal, args[1].intVal, args[2].realVal, args[3].realVal)); });
    vm_.RegisterNative("IssueInstantTargetOrder", [issueTarget](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 3) return JassValue::Bool(false); return JassValue::Bool(issueTarget(args[0].intVal, ParseOrderString(args[1].strVal), args[2].intVal)); });
    vm_.RegisterNative("IssueInstantTargetOrderById", [issueTarget](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 3) return JassValue::Bool(false); return JassValue::Bool(issueTarget(args[0].intVal, args[1].intVal, args[2].intVal)); });
    vm_.RegisterNative("IssueNeutralImmediateOrderById", [issueNoTarget](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 3) return JassValue::Bool(false); return JassValue::Bool(issueNoTarget(args[1].intVal, args[2].intVal)); });
    vm_.RegisterNative("IssueNeutralPointOrderById", [issuePoint](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 5) return JassValue::Bool(false); return JassValue::Bool(issuePoint(args[1].intVal, args[2].intVal, args[3].realVal, args[4].realVal)); });
    vm_.RegisterNative("IssueNeutralTargetOrderById", [issueTarget](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 4) return JassValue::Bool(false); return JassValue::Bool(issueTarget(args[1].intVal, args[2].intVal, args[3].intVal)); });
    vm_.RegisterNative("IssueNeutralImmediateOrder", [issueNoTarget](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 3) return JassValue::Bool(false); return JassValue::Bool(issueNoTarget(args[1].intVal, ParseOrderString(args[2].strVal))); });
    vm_.RegisterNative("IssueNeutralPointOrder", [issuePoint](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 5) return JassValue::Bool(false); return JassValue::Bool(issuePoint(args[1].intVal, ParseOrderString(args[2].strVal), args[3].realVal, args[4].realVal)); });
    vm_.RegisterNative("IssueNeutralTargetOrder", [issueTarget](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 4) return JassValue::Bool(false); return JassValue::Bool(issueTarget(args[1].intVal, ParseOrderString(args[2].strVal), args[3].intVal)); });
    vm_.RegisterNative("GroupImmediateOrderById", [data = data_, sim = sim_, groupUnits](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2 || args[1].intVal <= 0 || !sim) return JassValue::Bool(false); for (int uh : groupUnits(args[0].intVal)) { int uSim = data->GetSimHandle(uh, JassHandleInfo::UNIT); if (uSim >= 0) { if (CUnit* u = sim->GetUnit(uSim)) u->IssueOrder(std::make_unique<COrderNoTarget>(args[1].intVal)); } } return JassValue::Bool(true); });
    vm_.RegisterNative("GroupPointOrderById", [data = data_, sim = sim_, groupUnits](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 4 || args[1].intVal <= 0 || !sim) return JassValue::Bool(false); for (int uh : groupUnits(args[0].intVal)) { int uSim = data->GetSimHandle(uh, JassHandleInfo::UNIT); if (uSim >= 0) { if (CUnit* u = sim->GetUnit(uSim)) u->IssueOrder(std::make_unique<COrderTargetPoint>(args[1].intVal, args[2].realVal, args[3].realVal)); } } return JassValue::Bool(true); });
    vm_.RegisterNative("GroupPointOrderByIdLoc", [data = data_, sim = sim_, groupUnits, locationXY](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3 || args[1].intVal <= 0 || !sim) return JassValue::Bool(false);
        float x = 0, y = 0;
        if (!locationXY(args[2].intVal, x, y)) return JassValue::Bool(false);
        for (int uh : groupUnits(args[0].intVal)) {
            int uSim = data->GetSimHandle(uh, JassHandleInfo::UNIT);
            if (uSim >= 0) { if (CUnit* u = sim->GetUnit(uSim)) u->IssueOrder(std::make_unique<COrderTargetPoint>(args[1].intVal, x, y)); }
        }
        return JassValue::Bool(true);
    });
    vm_.RegisterNative("GroupPointOrderLoc", [data = data_, sim = sim_, groupUnits, locationXY](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3 || !sim) return JassValue::Bool(false);
        int orderId = ParseOrderString(args[1].strVal);
        if (orderId <= 0) return JassValue::Bool(false);
        float x = 0, y = 0;
        if (!locationXY(args[2].intVal, x, y)) return JassValue::Bool(false);
        for (int uh : groupUnits(args[0].intVal)) {
            int uSim = data->GetSimHandle(uh, JassHandleInfo::UNIT);
            if (uSim >= 0) { if (CUnit* u = sim->GetUnit(uSim)) u->IssueOrder(std::make_unique<COrderTargetPoint>(orderId, x, y)); }
        }
        return JassValue::Bool(true);
    });
    vm_.RegisterNative("GroupTargetOrderById", [data = data_, sim = sim_, groupUnits](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3 || args[1].intVal <= 0 || !sim) return JassValue::Bool(false);
        CWidget* target = ResolveOrderTargetWidget(data, sim, args[2].intVal);
        if (!target) return JassValue::Bool(false);
        for (int uh : groupUnits(args[0].intVal)) {
            int uSim = data->GetSimHandle(uh, JassHandleInfo::UNIT);
            if (uSim >= 0) { if (CUnit* u = sim->GetUnit(uSim)) u->IssueOrder(std::make_unique<COrderTargetWidget>(args[1].intVal, target)); }
        }
        return JassValue::Bool(true);
    });
}
void JassNatives::RegisterUnitStateNatives() {
    auto getUnit = [data = data_, sim = sim_](int unitHandle) -> CUnit* { if (!sim) return nullptr; int uSim = data->GetSimHandle(unitHandle, JassHandleInfo::UNIT); return uSim >= 0 ? sim->GetUnit(uSim) : nullptr; };
    vm_.RegisterNative("UnitAddType", [getUnit](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2) return JassValue::Bool(false); CUnit* u = getUnit(args[0].intVal); if (!u) return JassValue::Bool(false); u->AddUnitTypeFlag(args[1].intVal); return JassValue::Bool(true); });
    vm_.RegisterNative("UnitRemoveType", [getUnit](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2) return JassValue::Bool(false); CUnit* u = getUnit(args[0].intVal); if (!u) return JassValue::Bool(false); u->RemoveUnitTypeFlag(args[1].intVal); return JassValue::Bool(true); });
    vm_.RegisterNative("UnitMakeAbilityPermanent", [getUnit](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 3) return JassValue::Bool(false); CUnit* u = getUnit(args[0].intVal); if (!u) return JassValue::Bool(false); CAbility* ability = u->GetAbility(War3ID((uint32_t)args[2].intVal)); if (!ability) return JassValue::Bool(false); ability->SetPermanent(args[1].boolVal); return JassValue::Bool(true); });
    vm_.RegisterNative("UnitApplyTimedLife", [getUnit, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 3 || !sim) return JassValue::Null(); CUnit* u = getUnit(args[0].intVal); if (!u) return JassValue::Null(); if (const auto* mod = sim->GetSimulationModule()) { if (CBuff* timed = mod->CreateTimedLifeBuff((uint32_t)args[1].intVal, args[2].realVal)) { u->AddBuff(std::unique_ptr<CBuff>(timed), *sim); } } return JassValue::Null(); });
    vm_.RegisterNative("UnitPauseTimedLife", [getUnit](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2) return JassValue::Null(); CUnit* u = getUnit(args[0].intVal); if (!u) return JassValue::Null(); for (const auto& buff : u->GetBuffs()) { if (buff && buff->IsTimedLife()) { buff->SetSuspended(args[1].boolVal); } } return JassValue::Null(); });
    vm_.RegisterNative("UnitAddSleep", [getUnit](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2) return JassValue::Null(); if (CUnit* u = getUnit(args[0].intVal)) u->SetCanSleep(args[1].boolVal); return JassValue::Null(); });
    vm_.RegisterNative("UnitAddSleepPerm", [getUnit](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2) return JassValue::Null(); if (CUnit* u = getUnit(args[0].intVal)) u->SetCanSleepPerm(args[1].boolVal); return JassValue::Null(); });
    vm_.RegisterNative("UnitCanSleep", [getUnit](const std::vector<JassValue>& args) -> JassValue { if (args.empty()) return JassValue::Bool(false); CUnit* u = getUnit(args[0].intVal); return JassValue::Bool(u && u->CanSleep()); });
    vm_.RegisterNative("UnitCanSleepPerm", [getUnit](const std::vector<JassValue>& args) -> JassValue { if (args.empty()) return JassValue::Bool(false); CUnit* u = getUnit(args[0].intVal); return JassValue::Bool(u && u->CanSleepPerm()); });
    vm_.RegisterNative("UnitIsSleeping", [getUnit](const std::vector<JassValue>& args) -> JassValue { if (args.empty()) return JassValue::Bool(false); CUnit* u = getUnit(args[0].intVal); return JassValue::Bool(u && u->IsAsleep()); });
    vm_.RegisterNative("UnitWakeUp", [getUnit, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.empty() || !sim) return JassValue::Null(); if (CUnit* u = getUnit(args[0].intVal)) u->RemoveBuff(War3ID("BUsl"), *sim); return JassValue::Null(); });
    vm_.RegisterNative("UnitIgnoreAlarm", [getUnit](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2) return JassValue::Bool(false); CUnit* u = getUnit(args[0].intVal); if (!u) return JassValue::Bool(false); u->SetIgnoreAlarm(args[1].boolVal); return JassValue::Bool(true); });
    vm_.RegisterNative("UnitIgnoreAlarmToggled", [getUnit](const std::vector<JassValue>& args) -> JassValue { if (args.empty()) return JassValue::Bool(false); CUnit* u = getUnit(args[0].intVal); return JassValue::Bool(u && u->IsAlarmIgnored()); });
    vm_.RegisterNative("UnitShareVision", [data = data_, getUnit](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3) return JassValue::Null();
        int uSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (uSim < 0 || !getUnit(args[0].intVal)) return JassValue::Null();
        const int player = ResolvePlayerId(data, args[1].intVal);
        if (CUnit* u = getUnit(args[0].intVal)) u->SetSharesVisionWith(player, args[2].boolVal);
        if (args[2].boolVal) { data->unitSharedVision[uSim].insert(player); } else {
            data->unitSharedVision[uSim].erase(player);
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("UnitSetConstructionProgress", [getUnit](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2) return JassValue::Null(); CUnit* u = getUnit(args[0].intVal); if (!u) return JassValue::Null(); float buildTime = 1.0f; if (u->GetUnitType() && u->GetUnitType()->GetBuildTime() > 0) { buildTime = (float)u->GetUnitType()->GetBuildTime(); } u->SetConstructionProgress(args[1].intVal / 100.0f * buildTime); return JassValue::Null(); });
    vm_.RegisterNative("UnitSetUpgradeProgress", [getUnit](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2) return JassValue::Null(); if (CUnit* u = getUnit(args[0].intVal)) u->SetUpgradeProgress((float)args[1].intVal); return JassValue::Null(); });
    vm_.RegisterNative("UnitSetUsesAltIcon", [getUnit](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2) return JassValue::Null(); if (CUnit* u = getUnit(args[0].intVal)) u->SetUsesAltIcon(args[1].boolVal); return JassValue::Null(); });
    vm_.RegisterNative("UnitCountBuffsEx", [getUnit](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 8) return JassValue::Int(0); CUnit* u = getUnit(args[0].intVal); if (!u) return JassValue::Int(0); int count = 0; for (const auto& buff : u->GetBuffs()) { if (BuffExMatches(buff.get(), args[1].boolVal, args[2].boolVal, args[5].boolVal)) count++; } return JassValue::Int(count); });
    vm_.RegisterNative("UnitHasBuffsEx", [getUnit](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 8) return JassValue::Bool(false); CUnit* u = getUnit(args[0].intVal); if (!u) return JassValue::Bool(false); for (const auto& buff : u->GetBuffs()) { if (BuffExMatches(buff.get(), args[1].boolVal, args[2].boolVal, args[5].boolVal)) return JassValue::Bool(true); } return JassValue::Bool(false); });
    vm_.RegisterNative("UnitRemoveBuffs", [getUnit, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 3 || !sim) return JassValue::Null(); if (CUnit* u = getUnit(args[0].intVal)) { u->RemoveBuffsIf(*sim, args[1].boolVal, args[2].boolVal, false); } return JassValue::Null(); });
    vm_.RegisterNative("UnitRemoveBuffsEx", [getUnit, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 8 || !sim) return JassValue::Null(); if (CUnit* u = getUnit(args[0].intVal)) { u->RemoveBuffsIf(*sim, args[1].boolVal, args[2].boolVal, args[5].boolVal); } return JassValue::Null(); });
    vm_.RegisterNative("UnitDamagePoint", [data = data_, sim = sim_, getUnit](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 11 || !sim) return JassValue::Bool(false);
        CUnit* source = getUnit(args[0].intVal);
        const float delay = args[1].realVal;
        const float radius = args[2].realVal;
        const float x = args[3].realVal, y = args[4].realVal;
        const float amount = args[5].realVal;
        const bool isAttack = args[6].boolVal;
        const bool isRanged = args[7].boolVal;
        const CAttackType attackType = ToScriptAttackType(args[8].intVal);
        const CDamageType damageType = ToScriptDamageType(args[9].intVal);
        auto strike = [sim, source, radius, x, y, amount, isAttack, isRanged, attackType, damageType]() { for (CUnit* target : sim->GetUnits()) { if (!target || target->IsDead()) continue; const float dx = target->GetX() - x, dy = target->GetY() - y; if (dx * dx + dy * dy > radius * radius) continue; target->Damage(*sim, source, isAttack, isRanged, attackType, damageType, "", amount, 0); } };
        if (delay > 0.0f) { sim->AddDelayedAction(delay, std::move(strike)); } else {
            strike();
        }
        return JassValue::Bool(true);
    });
    vm_.RegisterNative("UnitAddIndicator", [data = data_, getUnit](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 5) return JassValue::Null();
        int uSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        if (uSim < 0 || !getUnit(args[0].intVal)) return JassValue::Null();
        JassNativesData::JassUnitIndicator ind;
        ind.red = args[1].intVal; ind.green = args[2].intVal;
        ind.blue = args[3].intVal; ind.alpha = args[4].intVal;
        data->unitIndicators[uSim] = ind;
        return JassValue::Null();
    });
    vm_.RegisterNative("SetUnitFog", [data = data_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 5) return JassValue::Null(); data->globalUnitFog.a = args[0].realVal; data->globalUnitFog.b = args[1].realVal; data->globalUnitFog.c = args[2].realVal; data->globalUnitFog.d = args[3].realVal; data->globalUnitFog.e = args[4].realVal; return JassValue::Null(); });
    vm_.RegisterNative("WaygateIsActive", [getUnit](const std::vector<JassValue>& args) -> JassValue { if (args.empty()) return JassValue::Bool(false); CUnit* u = getUnit(args[0].intVal); return JassValue::Bool(u && u->IsWaygateActive()); });
    vm_.RegisterNative("CachePlayerHeroData", [](const std::vector<JassValue>&) { return JassValue::Null(); });
}
namespace {
enum RefTargetType {
    RTT_AIR = 0, RTT_ALIVE = 1, RTT_ALLIES = 2, RTT_DEAD = 3, RTT_DEBRIS = 4,
    RTT_ENEMIES = 5, RTT_GROUND = 6, RTT_HERO = 7, RTT_INVULNERABLE = 8,
    RTT_ITEM = 9, RTT_MECHANICAL = 10, RTT_NEUTRAL = 11, RTT_NONE = 12,
    RTT_NONHERO = 13, RTT_NONSAPPER = 14, RTT_NOTSELF = 15, RTT_ORGANIC = 16,
    RTT_PLAYERUNITS = 17, RTT_SAPPER = 18, RTT_SELF = 19, RTT_STRUCTURE = 20,
    RTT_TERRAIN = 21, RTT_TREE = 22, RTT_VULNERABLE = 23, RTT_WALL = 24,
    RTT_WARD = 25, RTT_ANCIENT = 26, RTT_NONANCIENT = 27, RTT_FRIEND = 28,
    RTT_BRIDGE = 29, RTT_DECORATION = 30,
};
std::string CheckTargetAllowed(CSimulation& sim, CUnit* target, CUnit* source,
                               const std::unordered_set<int>& allowed, bool targeted = true) {
    if (!target || !source) return "Targetunit";
    const bool self = target->GetHandleId() == source->GetHandleId();
    auto has = [&](int t) { return allowed.count(t) > 0; };
    if (self && has(RTT_NOTSELF) && !has(RTT_SELF)) return "Notself";
    if (has(RTT_PLAYERUNITS) && source->GetPlayerIndex() != target->GetPlayerIndex()) { return "Targetowned"; }
    if (has(31) && target->IsMagicImmune()) return "Immunetomagic";
    if (targeted && (target->IsHidden() || target->IsInvisibleTo(source))) return "Targetunit";
    const CUnitType* type = target->GetUnitType();
    const bool isBuilding = target->IsBuilding();
    const bool isFlying = type && type->GetMovementType() == PathingGridMovementType::FLY;
    const bool tgtGround = !isBuilding && !isFlying;
    const bool tgtStructure = isBuilding;
    const bool tgtAir = isFlying;
    const bool anyAs = has(RTT_GROUND) || has(RTT_STRUCTURE) || has(RTT_AIR);
    const bool coversAs = ((has(RTT_GROUND) || !tgtGround) && (has(RTT_STRUCTURE) || !tgtStructure) &&
                           (has(RTT_AIR) || !tgtAir)) ||
                          !anyAs;
    if (!coversAs) {
        if (tgtGround && !has(RTT_GROUND)) return "Noground";
        if (tgtStructure && !has(RTT_STRUCTURE)) return "Nostructure";
        if (tgtAir && !has(RTT_AIR)) return "Noair";
        return "Notthisunit";
    }
    if (coversAs) {
        const CPlayer* selfPlayer = sim.GetPlayer(source->GetPlayerIndex());
        const CPlayer* tgtPlayer = sim.GetPlayer(target->GetPlayerIndex());
        const bool passive = selfPlayer && tgtPlayer &&
                             selfPlayer->HasAlliance(*tgtPlayer, CAllianceType::PASSIVE);
        if (!has(RTT_ENEMIES) || !passive || has(RTT_FRIEND)) {
            if (!has(RTT_FRIEND) || passive || has(RTT_ENEMIES)) {
                const auto& classes = type ? type->GetClassifications()
                                           : std::unordered_set<CUnitClassification>{};
                const bool mechanical = classes.count(CUnitClassification::MECHANICAL) > 0;
                const bool ancient = classes.count(CUnitClassification::ANCIENT) > 0;
                if (!has(RTT_MECHANICAL) || mechanical) {
                    if (!has(RTT_ORGANIC) || !mechanical) {
                        if (!has(RTT_ANCIENT) || ancient) {
                            if (!has(RTT_NONANCIENT) || !ancient) {
                                const bool invuln = target->IsInvulnerable();
                                if ((!invuln && (has(RTT_VULNERABLE) || !has(RTT_INVULNERABLE))) ||
                                    (invuln && has(RTT_INVULNERABLE))) {
                                    if (!has(RTT_HERO) || target->IsHero()) {
                                        if (!has(RTT_NONHERO) || !target->IsHero()) {
                                            if (target->IsDead()) {
                                                if (type && type->CanRaise() && type->CanDecay() &&
                                                    target->IsBoneCorpse()) {
                                                    if (has(RTT_DEAD)) return "";
                                                    return "Notcorpse";
                                                }
                                                return "Targetunit";
                                            }
                                            if (!has(RTT_DEAD) || has(RTT_ALIVE)) return "";
                                            return "Targetcorpse";
                                        }
                                        return "Nohero";
                                    }
                                    return "Targethero";
                                }
                                return invuln ? "Notinvulnerable" : "Notthisunit";
                            }
                            return "Targetancient";
                        }
                        return "Notancient";
                    }
                    return "Notmechanical";
                }
                return "Notorganic";
            }
            return "Targetally";
        }
        return "Targetenemy";
    }
    if (tgtGround && !has(RTT_GROUND)) return "Noground";
    if (tgtStructure && !has(RTT_STRUCTURE)) return "Nostructure";
    if (tgtAir && !has(RTT_AIR)) return "Noair";
    return "Notthisunit";
}
}
void JassNatives::RegisterTargetingNatives() {
    vm_.RegisterNative("CreateTargetTypes", [data = data_](const std::vector<JassValue>&) -> JassValue { if (!data) return JassValue::Int(0); int id = data->nextTargetTypeSetId++; data->targetTypeSets[id] = {}; return JassValue::Int(data->AllocHandle(JassHandleInfo::TARGET_TYPES, id)); });
    vm_.RegisterNative("TargetTypesAdd", [data = data_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2 || !data) return JassValue::Null(); int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::TARGET_TYPES); if (simId < 0) return JassValue::Null(); data->targetTypeSets[simId].insert(args[1].intVal); return JassValue::Null(); });
    vm_.RegisterNative("TargetTypesRemove", [data = data_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2 || !data) return JassValue::Null(); int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::TARGET_TYPES); if (simId < 0) return JassValue::Null(); auto it = data->targetTypeSets.find(simId); if (it != data->targetTypeSets.end()) it->second.erase(args[1].intVal); return JassValue::Null(); });
    vm_.RegisterNative("TargetTypesContains", [data = data_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2 || !data) return JassValue::Bool(false); int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::TARGET_TYPES); if (simId < 0) return JassValue::Bool(false); auto it = data->targetTypeSets.find(simId); return JassValue::Bool(it != data->targetTypeSets.end() && it->second.count(args[1].intVal) > 0); });
    vm_.RegisterNative("ParseTargetTypes", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty() || !data) return JassValue::Int(0);
        int listSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::STRING_LIST);
        if (listSim < 0) return JassValue::Int(0);
        auto lit = data->stringLists.find(listSim);
        if (lit == data->stringLists.end()) return JassValue::Int(0);
        int id = data->nextTargetTypeSetId++;
        auto& set = data->targetTypeSets[id];
        for (const std::string& raw : lit->second) {
            const int ordinal = ParseRefTargetOrdinal(raw);
            if (ordinal >= 0) set.insert(ordinal);
        }
        return JassValue::Int(data->AllocHandle(JassHandleInfo::TARGET_TYPES, id));
    });
    auto checkTarget = [data = data_, sim = sim_](const std::vector<JassValue>& args) -> std::string {
        if (args.size() < 3 || !sim) return "Targetunit";
        int setSim = data->GetSimHandle(args[2].intVal, JassHandleInfo::TARGET_TYPES);
        auto sit = setSim >= 0 ? data->targetTypeSets.find(setSim) : data->targetTypeSets.end();
        static const std::unordered_set<int> kEmpty;
        const std::unordered_set<int>& allowed = sit != data->targetTypeSets.end() ? sit->second : kEmpty;
        int tSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        int cSim = data->GetSimHandle(args[1].intVal, JassHandleInfo::UNIT);
        CUnit* target = tSim >= 0 ? sim->GetUnit(tSim) : nullptr;
        CUnit* source = cSim >= 0 ? sim->GetUnit(cSim) : nullptr;
        if (target && source) return CheckTargetAllowed(*sim, target, source, allowed);
        if (!target) {
            const int iSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::ITEM);
            if (iSim >= 0) return sim->GetItem(iSim) ? "" : "Targetunit";
            const int dSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::DESTRUCTABLE);
            if (dSim >= 0) {
                CDestructable* dest = sim->GetDestructable(dSim);
                return (dest && !dest->IsDead()) ? "" : "Targetunit";
            }
        }
        return "Targetunit";
    };
    vm_.RegisterNative("IsValidTarget", [checkTarget](const std::vector<JassValue>& args) -> JassValue { return JassValue::Bool(checkTarget(args).empty()); });
    vm_.RegisterNative("IsUnitValidTarget", [checkTarget](const std::vector<JassValue>& args) -> JassValue { return JassValue::Bool(checkTarget(args).empty()); });
    vm_.RegisterNative("GetTargetError", [checkTarget](const std::vector<JassValue>& args) -> JassValue { return JassValue::Str(checkTarget(args)); });
    vm_.RegisterNative("GetUnitTargetError", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3 || !sim) return JassValue::Str("Targetunit");
        int setSim = data->GetSimHandle(args[2].intVal, JassHandleInfo::TARGET_TYPES);
        auto sit = setSim >= 0 ? data->targetTypeSets.find(setSim) : data->targetTypeSets.end();
        static const std::unordered_set<int> kEmpty;
        const std::unordered_set<int>& allowed = sit != data->targetTypeSets.end() ? sit->second : kEmpty;
        int tSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        int cSim = data->GetSimHandle(args[1].intVal, JassHandleInfo::UNIT);
        CUnit* target = tSim >= 0 ? sim->GetUnit(tSim) : nullptr;
        CUnit* source = cSim >= 0 ? sim->GetUnit(cSim) : nullptr;
        if (!target || !source) return JassValue::Str("Targetunit");
        const bool targeted = args.size() < 4 || args[3].boolVal;
        return JassValue::Str(CheckTargetAllowed(*sim, target, source, allowed, targeted));
    });
}
namespace {
bool StatBonusAuraSlot(int type, CAuraEffectType& slot, bool& percent) {
    switch (type) {
        case 0: slot = CAuraEffectType::DAMAGE; percent = false; return true;
        case 1: slot = CAuraEffectType::DAMAGE; percent = true; return true;
        case 2: slot = CAuraEffectType::DAMAGE; percent = false; return true;
        case 3: slot = CAuraEffectType::DAMAGE; percent = true; return true;
        case 4: slot = CAuraEffectType::ATTACK_SPEED; percent = false; return true;
        case 5: slot = CAuraEffectType::ARMOR; percent = false; return true;
        case 6: slot = CAuraEffectType::ARMOR; percent = true; return true;
        case 7: slot = CAuraEffectType::LIFE_REGEN; percent = false; return true;
        case 8: slot = CAuraEffectType::LIFE_REGEN; percent = true; return true;
        case 9: slot = CAuraEffectType::LIFE_REGEN; percent = true; return true;
        case 10: slot = CAuraEffectType::MANA_REGEN; percent = false; return true;
        case 11: slot = CAuraEffectType::MANA_REGEN; percent = true; return true;
        case 12: slot = CAuraEffectType::MANA_REGEN; percent = true; return true;
        case 13: slot = CAuraEffectType::MOVE_SPEED; percent = false; return true;
        case 14: slot = CAuraEffectType::MOVE_SPEED; percent = true; return true;
        case 22: slot = CAuraEffectType::DAMAGE; percent = false; return true;
        case 23: slot = CAuraEffectType::DAMAGE; percent = true; return true;
        default: return false;
    }
}
}
void JassNatives::RecomputeMaxStatBonus(int unitSim, int type) {
    if (!data_ || !sim_) return;
    CUnit* unit = sim_->GetUnit(unitSim);
    if (!unit) return;
    float best = 0.0f;
    bool have = false;
    for (const auto& [id, bonus] : data_->statBonuses) {
        (void)id;
        if (!bonus || bonus->type != type || bonus->holders.count(unitSim) == 0) continue;
        float v = bonus->value;
        if (type == 19 || type == 21) {
            const CUnitType* ut = unit->GetUnitType();
            const float base = ut ? (type == 19 ? (float)ut->GetMaxLife() : (float)ut->GetManaMaximum()) : 0.0f;
            v *= base;
        }
        if (!have || v > best) {
            best = v;
            have = true;
        }
    }
    const int64_t key = (static_cast<int64_t>(unitSim) << 32) | (uint32_t)type;
    const float applied = data_->maxStatApplied.count(key) > 0 ? data_->maxStatApplied[key] : 0.0f;
    const float target = have ? best : 0.0f;
    if (target != applied) {
        const float delta = target - applied;
        if (type == 18 || type == 19) { unit->AddBonusMaxLife(delta); } else {
            unit->AddBonusMaxMana((int)std::lround(delta));
        }
        if (have) { data_->maxStatApplied[key] = target; } else {
            data_->maxStatApplied.erase(key);
        }
    }
}
void JassNatives::RecomputeStateMod(int unitSim, int type) {
    if (!data_ || !sim_) return;
    CUnit* unit = sim_->GetUnit(unitSim);
    if (!unit) return;
    bool on = false;
    for (const auto& [id, mod] : data_->stateMods) {
        (void)id;
        if (mod && mod->type == type && mod->holders.count(unitSim) > 0 && mod->value != 0) {
            on = true;
            break;
        }
    }
    switch (type) {
        case 0: unit->SetEthereal(on); break;
        case 4: unit->SetMagicImmune(on); break;
        case 17: unit->SetInvulnerable(on); break;
        case 18: unit->SetInvisible(on); break;
        case 19: unit->SetDetector(on); break;
        default: break;
    }
}
void JassNatives::RegisterBonusNatives() {
    auto getBonus = [data = data_](int handle) -> std::shared_ptr<JassNativesData::JassStatBonus> { if (!data) return nullptr; int simId = data->GetSimHandle(handle, JassHandleInfo::STAT_BONUS); if (simId < 0) return nullptr; auto it = data->statBonuses.find(simId); return it != data->statBonuses.end() ? it->second : nullptr; };
    auto getMod = [data = data_](int handle) -> std::shared_ptr<JassNativesData::JassStateMod> { if (!data) return nullptr; int simId = data->GetSimHandle(handle, JassHandleInfo::STATE_MOD); if (simId < 0) return nullptr; auto it = data->stateMods.find(simId); return it != data->stateMods.end() ? it->second : nullptr; };
    auto applyBonus = [this](int bonusSim, int unitSim) {
        if (!data_ || !sim_) return;
        auto bit = data_->statBonuses.find(bonusSim);
        CUnit* unit = sim_->GetUnit(unitSim);
        if (bit == data_->statBonuses.end() || !bit->second || !unit) return;
        const auto& bonus = bit->second;
        CAuraEffectType slot = CAuraEffectType::DAMAGE;
        bool percent = false;
        if (StatBonusAuraSlot(bonus->type, slot, percent)) { unit->AddAuraStatBonus(slot, percent, War3ID((uint32_t)bonusSim), bonus->value); } else if (bonus->type >= 18 && bonus->type <= 21) { RecomputeMaxStatBonus(unitSim, bonus->type); }
    };
    auto unapplyBonus = [this](int bonusSim, int unitSim) {
        if (!data_ || !sim_) return;
        auto bit = data_->statBonuses.find(bonusSim);
        CUnit* unit = sim_->GetUnit(unitSim);
        if (bit == data_->statBonuses.end() || !bit->second || !unit) return;
        const auto& bonus = bit->second;
        CAuraEffectType slot = CAuraEffectType::DAMAGE;
        bool percent = false;
        if (StatBonusAuraSlot(bonus->type, slot, percent)) { unit->RemoveAuraStatBonus(slot, percent, War3ID((uint32_t)bonusSim)); } else if (bonus->type >= 18 && bonus->type <= 21) { RecomputeMaxStatBonus(unitSim, bonus->type); }
    };
    vm_.RegisterNative("CreateNonStackingStatBonus", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3 || !data) return JassValue::Int(0);
        auto bonus = std::make_shared<JassNativesData::JassStatBonus>();
        bonus->type = args[0].intVal;
        bonus->key = args[1].strVal;
        bonus->value = args[2].type == JassValue::REAL ? args[2].realVal : (float)args[2].intVal;
        int id = data->nextStatBonusId++;
        data->statBonuses[id] = std::move(bonus);
        return JassValue::Int(data->AllocHandle(JassHandleInfo::STAT_BONUS, id));
    });
    vm_.RegisterNative("AddUnitNonStackingStatBonus", [data = data_, applyBonus](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !data) return JassValue::Null();
        int uSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        int bSim = data->GetSimHandle(args[1].intVal, JassHandleInfo::STAT_BONUS);
        if (uSim < 0 || bSim < 0) return JassValue::Null();
        auto it = data->statBonuses.find(bSim);
        if (it == data->statBonuses.end()) return JassValue::Null();
        it->second->holders.insert(uSim);
        applyBonus(bSim, uSim);
        return JassValue::Null();
    });
    vm_.RegisterNative("RemoveUnitNonStackingStatBonus", [data = data_, unapplyBonus](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !data) return JassValue::Null();
        int uSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        int bSim = data->GetSimHandle(args[1].intVal, JassHandleInfo::STAT_BONUS);
        if (uSim < 0 || bSim < 0) return JassValue::Null();
        auto it = data->statBonuses.find(bSim);
        if (it == data->statBonuses.end()) return JassValue::Null();
        it->second->holders.erase(uSim);
        unapplyBonus(bSim, uSim);
        return JassValue::Null();
    });
    vm_.RegisterNative("UpdateNonStackingStatBonus", [data = data_, applyBonus](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !data) return JassValue::Null();
        int bSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::STAT_BONUS);
        auto it = bSim >= 0 ? data->statBonuses.find(bSim) : data->statBonuses.end();
        if (it == data->statBonuses.end()) return JassValue::Null();
        it->second->value = args[1].type == JassValue::REAL ? args[1].realVal : (float)args[1].intVal;
        const std::vector<int> holders(it->second->holders.begin(), it->second->holders.end());
        for (int uSim : holders) applyBonus(bSim, uSim);
        return JassValue::Null();
    });
    vm_.RegisterNative("GetNonStackingStatBonusType", [getBonus](const std::vector<JassValue>& args) -> JassValue { if (args.empty()) return JassValue::Int(0); auto bonus = getBonus(args[0].intVal); return JassValue::Int(bonus ? bonus->type : 0); });
    vm_.RegisterNative("CreateStateMod", [data = data_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2 || !data) return JassValue::Int(0); auto mod = std::make_shared<JassNativesData::JassStateMod>(); mod->type = args[0].intVal; mod->value = args[1].intVal; int id = data->nextStateModId++; data->stateMods[id] = std::move(mod); return JassValue::Int(data->AllocHandle(JassHandleInfo::STATE_MOD, id)); });
    vm_.RegisterNative("AddUnitStateMod", [this](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !data_) return JassValue::Null();
        int uSim = data_->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        int mSim = data_->GetSimHandle(args[1].intVal, JassHandleInfo::STATE_MOD);
        if (uSim < 0 || mSim < 0) return JassValue::Null();
        auto it = data_->stateMods.find(mSim);
        if (it == data_->stateMods.end()) return JassValue::Null();
        it->second->holders.insert(uSim);
        RecomputeStateMod(uSim, it->second->type);
        return JassValue::Null();
    });
    vm_.RegisterNative("RemoveUnitStateMod", [this](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !data_) return JassValue::Null();
        int uSim = data_->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        int mSim = data_->GetSimHandle(args[1].intVal, JassHandleInfo::STATE_MOD);
        if (uSim < 0 || mSim < 0) return JassValue::Null();
        auto it = data_->stateMods.find(mSim);
        if (it == data_->stateMods.end()) return JassValue::Null();
        it->second->holders.erase(uSim);
        RecomputeStateMod(uSim, it->second->type);
        return JassValue::Null();
    });
    vm_.RegisterNative("UpdateStateMod", [this](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !data_) return JassValue::Null();
        int mSim = data_->GetSimHandle(args[0].intVal, JassHandleInfo::STATE_MOD);
        auto it = mSim >= 0 ? data_->stateMods.find(mSim) : data_->stateMods.end();
        if (it == data_->stateMods.end()) return JassValue::Null();
        it->second->value = args[1].intVal;
        const std::vector<int> holders(it->second->holders.begin(), it->second->holders.end());
        for (int uSim : holders) RecomputeStateMod(uSim, it->second->type);
        return JassValue::Null();
    });
    vm_.RegisterNative("RecomputeStateModsOnUnit", [this](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2 || !data_) return JassValue::Null(); int uSim = data_->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT); if (uSim < 0) return JassValue::Null(); RecomputeStateMod(uSim, args[1].intVal); return JassValue::Null(); });
}
