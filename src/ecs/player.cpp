#include "player.h"
#include "csimulation.h"
#include "unit.h"

namespace {
uint64_t MakeUnitUpgradeKey(int unitHandleId, War3ID upgradeId) {
    return (static_cast<uint64_t>(static_cast<uint32_t>(unitHandleId)) << 32) |
           static_cast<uint64_t>(upgradeId.value);
}
} // namespace

CPlayer::CPlayer(int id, const std::string& name, CMapControl controller)
    : id_(id), name_(name), controller_(controller), color_(id) {}

int CPlayer::GetUpkeepLevel(const CSimulation* sim) const {
    if (sim && sim->GetSimulationModule()) {
        return sim->GetSimulationModule()->ComputeUpkeepLevel(*this);
    }
    if (foodUsed_ > 80) return 2;
    if (foodUsed_ > 50) return 1;
    return 0;
}

int CPlayer::ApplyUpkeepGoldTax(int amount, const CSimulation* sim) const {
    if (sim && sim->GetSimulationModule()) {
        return sim->GetSimulationModule()->ApplyUpkeepGoldTax(*this, amount);
    }
    if (amount <= 0) return amount;
    const int upkeepLvl = GetUpkeepLevel(nullptr);
    const int taxPct = goldUpkeepRate_ > 0 ? goldUpkeepRate_
                     : (upkeepLvl == 2 ? 60 : upkeepLvl == 1 ? 30 : 0);
    if (taxPct <= 0) return amount;
    if (taxPct >= 100) return 0;
    return amount - amount * taxPct / 100;
}

bool CPlayer::IsTechUnlocked(War3ID id) const {
    auto it = techtreeUnlocked_.find(id.value);
    return it != techtreeUnlocked_.end() && it->second > 0;
}

void CPlayer::AddTechUnlocked(CSimulation& simulation, War3ID id) {
    const int oldLevel = GetTechLevel(id);
    const int newLevel = ++techtreeUnlocked_[id.value];
    if (auto* mod = simulation.GetSimulationModule()) {
        mod->OnPlayerTechLevelChanged(simulation, *this, id.value, oldLevel, newLevel);
    }
    simulation.Events().onPlayerTechLevelChanged.Emit(id_, id.value, newLevel);
}

void CPlayer::RemoveTechUnlocked(CSimulation& simulation, War3ID id) {
    auto it = techtreeUnlocked_.find(id.value);
    if (it == techtreeUnlocked_.end()) return;
    const int oldLevel = it->second;
    int newLevel = 0;
    if (it->second <= 1) {
        techtreeUnlocked_.erase(it);
    } else {
        newLevel = --it->second;
    }
    if (auto* mod = simulation.GetSimulationModule()) {
        mod->OnPlayerTechLevelChanged(simulation, *this, id.value, oldLevel, newLevel);
    }
    simulation.Events().onPlayerTechLevelChanged.Emit(id_, id.value, newLevel);
}

int CPlayer::GetTechLevel(War3ID id) const {
    auto it = techtreeUnlocked_.find(id.value);
    return (it != techtreeUnlocked_.end()) ? it->second : 0;
}

int CPlayer::GetEffectiveTechLevel(const CSimulation& simulation, War3ID id) const {
    if (const auto* mod = simulation.GetSimulationModule()) {
        return mod->GetEffectiveTechLevel(simulation, id_, id.value);
    }
    return GetTechLevel(id);
}

void CPlayer::SetTechLevel(War3ID id, int level) {
    if (level <= 0) {
        techtreeUnlocked_.erase(id.value);
    } else {
        techtreeUnlocked_[id.value] = level;
    }
}

void CPlayer::SetTechLevel(CSimulation& simulation, War3ID id, int level) {
    const int oldLevel = GetTechLevel(id);
    const int clamped = std::max(0, level);
    SetTechLevel(id, clamped);
    if (oldLevel != clamped) {
        if (auto* mod = simulation.GetSimulationModule()) {
            mod->OnPlayerTechLevelChanged(simulation, *this, id.value, oldLevel, clamped);
        }
        simulation.Events().onPlayerTechLevelChanged.Emit(id_, id.value, clamped);
    }
}

int CPlayer::GetTechMaxAllowed(War3ID id) const {
    auto it = techtreeMaxAllowed_.find(id.value);
    return (it != techtreeMaxAllowed_.end()) ? it->second : -1;
}

void CPlayer::SetTechMaxAllowed(War3ID id, int level) {
    techtreeMaxAllowed_[id.value] = level;
}

bool CPlayer::IsTechtreeAllowedByMax(War3ID id) const {
    const int maxAllowed = GetTechMaxAllowed(id);
    if (maxAllowed < 0) return true;
    auto it = techtreeUnlocked_.find(id.value);
    const int have = (it != techtreeUnlocked_.end()) ? it->second : 0;
    return have < maxAllowed;
}

int CPlayer::GetAppliedUnitUpgradeLevel(int unitHandleId, War3ID upgradeId) const {
    auto it = appliedUnitUpgradeLevels_.find(MakeUnitUpgradeKey(unitHandleId, upgradeId));
    return (it != appliedUnitUpgradeLevels_.end()) ? it->second : 0;
}

void CPlayer::SetAppliedUnitUpgradeLevel(int unitHandleId, War3ID upgradeId, int level) {
    const uint64_t key = MakeUnitUpgradeKey(unitHandleId, upgradeId);
    if (level <= 0) {
        appliedUnitUpgradeLevels_.erase(key);
    } else {
        appliedUnitUpgradeLevels_[key] = level;
    }
}

void CPlayer::ClearAppliedUnitUpgrades(int unitHandleId) {
    const uint32_t uHandle = static_cast<uint32_t>(unitHandleId);
    for (auto it = appliedUnitUpgradeLevels_.begin(); it != appliedUnitUpgradeLevels_.end();) {
        if (static_cast<uint32_t>(it->first >> 32) == uHandle) {
            it = appliedUnitUpgradeLevels_.erase(it);
        } else {
            ++it;
        }
    }
}

bool CPlayer::HasAlliance(const CPlayer& other, CAllianceType type) const {
    if (id_ == other.GetId()) {
        return true;
    }
    if (other.GetId() < 0 || other.GetId() >= 24) return false;
    int bit = (int)type + other.GetId() * 16;
    int idx = bit / 32;
    if (idx < 0 || idx >= 24) return false;
    return (allianceData_[idx] & (1 << (bit % 32))) != 0;
}

void CPlayer::SetAlliance(int otherPlayerId, CAllianceType type, bool flag) {
    if (otherPlayerId < 0 || otherPlayerId >= 24) return;
    int bit = (int)type + otherPlayerId * 16;
    int idx = bit / 32;
    if (idx < 0 || idx >= 24) return;
    if (flag)
        allianceData_[idx] |= (1 << (bit % 32));
    else
        allianceData_[idx] &= ~(1 << (bit % 32));
}

void CPlayer::AddUnit(CUnit* unit) {
    units_.push_back(unit);
}

void CPlayer::RemoveUnit(CUnit* unit) {
    if (unit) {
        ClearAppliedUnitUpgrades(unit->GetHandleId());
    }
    for (auto it = units_.begin(); it != units_.end(); ++it) {
        if (*it == unit) { units_.erase(it); break; }
    }
}

void CPlayer::AddHero(CUnit* hero) {
    heroes_.push_back(hero);
}

void CPlayer::RemoveHero(CUnit* hero) {
    for (auto it = heroes_.begin(); it != heroes_.end(); ++it) {
        if (*it == hero) { heroes_.erase(it); break; }
    }
}
