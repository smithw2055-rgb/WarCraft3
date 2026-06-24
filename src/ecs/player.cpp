#include "player.h"
#include "csimulation.h"

CPlayer::CPlayer(int id, const std::string& name, CMapControl controller)
    : id_(id), name_(name), controller_(controller) {}

bool CPlayer::IsTechUnlocked(War3ID id) const {
    auto it = techtreeUnlocked_.find(id.value);
    return it != techtreeUnlocked_.end() && it->second > 0;
}

void CPlayer::AddTechUnlocked(CSimulation& simulation, War3ID id) {
    techtreeUnlocked_[id.value]++;
    (void)simulation;
}

int CPlayer::GetTechLevel(War3ID id) const {
    auto it = techtreeUnlocked_.find(id.value);
    return (it != techtreeUnlocked_.end()) ? it->second : 0;
}

void CPlayer::SetTechLevel(War3ID id, int level) {
    techtreeUnlocked_[id.value] = level;
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
