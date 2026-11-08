#include "ability.h"
#include "ecs/unit.h"
#include "ecs/csimulation.h"

const std::unordered_set<War3ID>& CAbility::EmptyWar3IdSet() {
    static const std::unordered_set<War3ID> kEmpty;
    return kEmpty;
}

const HeroStatValue& CAbility::EmptyHeroStat() {
    static const HeroStatValue kEmpty{};
    return kEmpty;
}

const std::string& CAbility::EmptyProperName() {
    static const std::string kEmpty;
    return kEmpty;
}

CAbility::CAbility(int handleId, War3ID code, CAbilityCategory category)
    : handleId_(handleId), code_(code), alias_(code), category_(category),
      level_(1), physical_(0), permanent_(0), iconShowing_(1),
      autocastActive_(false), passiveSpell_(false) {}

void CAbility::SetDisabled(bool disabled, CAbilityDisableType type) {
    if (disabled) {
        disabled_ |= static_cast<uint8_t>(type);
    } else {
        disabled_ &= ~static_cast<uint8_t>(type);
    }
}

void CAbility::OnAdd(CSimulation& game, CUnit* unit) {
    (void)game; (void)unit;
}

void CAbility::OnRemove(CSimulation& game, CUnit* unit) {
    (void)game; (void)unit;
}

void CAbility::OnTick(CSimulation& game, CUnit* unit) {
    (void)game; (void)unit;
}

void CAbility::OnDeath(CSimulation& game, CUnit* unit) {
    (void)game; (void)unit;
}

void CAbility::OnLevelChanged(CSimulation& game, CUnit* unit) {
    OnRemove(game, unit);
    if (!IsDisabled()) {
        OnAdd(game, unit);
    }
}

bool CAbility::CheckCanUse(CSimulation& game, CUnit* unit, int orderId) const {
    (void)game; (void)unit; (void)orderId;
    if (IsDisabled()) return false;
    return true;
}

bool CAbility::CheckCanTarget(CSimulation& game, CUnit* unit, int orderId, CWidget* target) const {
    (void)game; (void)unit; (void)orderId; (void)target;
    return false;
}

bool CAbility::CheckCanTargetPoint(CSimulation& game, CUnit* unit, int orderId, float x, float y) const {
    (void)game; (void)unit; (void)orderId; (void)x; (void)y;
    return false;
}

bool CAbility::CheckCanTargetNoTarget(CSimulation& game, CUnit* unit, int orderId) const {
    (void)game; (void)unit; (void)orderId;
    return false;
}

bool CAbility::IsRequirementsMet(CSimulation& game, CUnit* unit) const {
    (void)game; (void)unit;
    return true;
}

bool CAbility::CheckBeforeQueue(CSimulation& game, CUnit* caster, int orderId, CWidget* target) {
    (void)game; (void)caster; (void)orderId; (void)target;
    return true; // true = allow queueing
}

bool CAbility::CheckBeforeQueue(CSimulation& game, CUnit* caster, int orderId, float x, float y) {
    (void)game; (void)caster; (void)orderId; (void)x; (void)y;
    return true;
}

bool CAbility::CheckBeforeQueueNoTarget(CSimulation& game, CUnit* caster, int orderId) {
    (void)game; (void)caster; (void)orderId;
    return true;
}

CBehavior* CAbility::Begin(CSimulation& game, CUnit* caster, int orderId, CWidget* target) {
    (void)game; (void)caster; (void)orderId; (void)target;
    return nullptr;
}

CBehavior* CAbility::Begin(CSimulation& game, CUnit* caster, int orderId, float x, float y) {
    (void)game; (void)caster; (void)orderId; (void)x; (void)y;
    return nullptr;
}

CBehavior* CAbility::BeginNoTarget(CSimulation& game, CUnit* caster, int orderId) {
    (void)game; (void)caster; (void)orderId;
    return nullptr;
}
