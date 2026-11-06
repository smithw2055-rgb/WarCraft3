#include "ecs/attribute_aggregator.h"
#include "ecs/unit_type.h"
#include <algorithm>
#include <cmath>

void UnitAttributeAggregator::InitFromUnitType(const CUnitType* unitType) {
    if (!unitType) return;
    baseSpeed_ = unitType->GetSpeed();
    baseMaxLife_ = unitType->GetMaxLife();
    baseMaxMana_ = unitType->GetManaMaximum();
    currentDefense_ = unitType->GetDefense();
}

float UnitAttributeAggregator::ComputeEffectiveSpeed() const {
    if (baseSpeed_ <= 0) return 0.0f;
    float base = (float)baseSpeed_ + (float)bonusSpeed_;
    float percent = auraMoveSpeedPercent_;
    for (const auto& [id, mod] : speedModifiers_) {
        percent += mod;
    }
    float finalSpeed = base * (1.0f + percent);
    return std::clamp(finalSpeed, 150.0f, 522.0f);
}

float UnitAttributeAggregator::ComputeMaxLife() const {
    float total = (float)baseMaxLife_ + heroBonusLife_ + bonusMaxLife_;
    return (total < 1.0f) ? 1.0f : total;
}

int UnitAttributeAggregator::ComputeMaxMana() const {
    int total = baseMaxMana_ + heroBonusMana_ + bonusMaxMana_;
    return (total < 0) ? 0 : total;
}

void UnitAttributeAggregator::SetNonStackingDamageBonus(War3ID key, int flatValue) {
    if (flatValue == 0) {
        nsDamageFlat_.erase(key);
        return;
    }
    nsDamageFlat_[key] = flatValue;
}

void UnitAttributeAggregator::SetNonStackingDamagePctBonus(War3ID key, int flatValue) {
    if (flatValue == 0) {
        nsDamagePct_.erase(key);
        return;
    }
    nsDamagePct_[key] = flatValue;
}

void UnitAttributeAggregator::ClearNonStackingDamageBonus(War3ID key) {
    nsDamageFlat_.erase(key);
    nsDamagePct_.erase(key);
}

int UnitAttributeAggregator::GetNonStackingDamageTotal() const {
    int total = 0;
    for (const auto& [key, value] : nsDamageFlat_) total += value;
    for (const auto& [key, value] : nsDamagePct_) total += value;
    return total;
}

void UnitAttributeAggregator::SetNonStackingAttackSpeed(War3ID key, float bonus) {
    if (bonus == 0.0f) {
        nsAtkSpeed_.erase(key);
        return;
    }
    nsAtkSpeed_[key] = bonus;
}

void UnitAttributeAggregator::ClearNonStackingAttackSpeed(War3ID key) {
    nsAtkSpeed_.erase(key);
}

float UnitAttributeAggregator::GetNonStackingAttackSpeedTotal() const {
    float total = 0.0f;
    for (const auto& [key, value] : nsAtkSpeed_) total += value;
    return total;
}

float UnitAttributeAggregator::ComputeAttackSpeedMultiplier() const {
    const float bonus = attackSpeedStackBonus_ + GetNonStackingAttackSpeedTotal();
    const float clamped = std::max(-0.9f, std::min(4.0f, bonus));
    return (1.0f + clamped) * attackSpeedMul_;
}

float UnitAttributeAggregator::GetAttackRangeBonus(int index) const {
    if (index >= 0 && index < (int)attackRangeBonus_.size()) {
        return attackRangeBonus_[(size_t)index];
    }
    return 0.0f;
}

void UnitAttributeAggregator::AddAttackRangeBonus(int index, float v) {
    if (index < 0) return;
    if (index >= (int)attackRangeBonus_.size()) {
        attackRangeBonus_.resize((size_t)index + 1, 0.0f);
    }
    attackRangeBonus_[(size_t)index] += v;
}

void UnitAttributeAggregator::AddAttackRangeBonusAll(float v, size_t attackCount) {
    size_t count = std::max<size_t>(1, attackCount);
    if (!attackRangeBonus_.empty()) {
        count = std::max(count, attackRangeBonus_.size());
    }
    if (attackRangeBonus_.size() < count) {
        attackRangeBonus_.resize(count, 0.0f);
    }
    for (auto& b : attackRangeBonus_) b += v;
}

void UnitAttributeAggregator::AddEvasionSource(War3ID id, float chance) {
    for (auto& src : evasionSources_) {
        if (src.id == id) {
            src.chance = chance;
            return;
        }
    }
    evasionSources_.push_back({id, chance});
}

void UnitAttributeAggregator::RemoveEvasionSource(War3ID id) {
    evasionSources_.erase(
        std::remove_if(evasionSources_.begin(), evasionSources_.end(),
                       [&](const UnitEvasionSource& src) { return src.id == id; }),
        evasionSources_.end());
}

float UnitAttributeAggregator::ComputeEvasionChance() const {
    float survive = 1.0f;
    for (const auto& src : evasionSources_) {
        survive *= (1.0f - src.chance);
    }
    return 1.0f - survive;
}

void UnitAttributeAggregator::SetEvasionChance(float v) {
    evasionSources_.clear();
    if (v != 0.0f) evasionSources_.push_back({War3ID(0u), v});
}

void UnitAttributeAggregator::AddCritSource(War3ID id, float chance, float multiplier) {
    for (auto& src : critSources_) {
        if (src.id == id) {
            src.chance = chance;
            src.multiplier = multiplier;
            return;
        }
    }
    critSources_.push_back({id, chance, multiplier});
}

void UnitAttributeAggregator::RemoveCritSource(War3ID id) {
    critSources_.erase(
        std::remove_if(critSources_.begin(), critSources_.end(),
                       [&](const UnitCritSource& src) { return src.id == id; }),
        critSources_.end());
}

float UnitAttributeAggregator::ComputeCritChance() const {
    float best = 0.0f;
    for (const auto& src : critSources_) {
        if (src.chance > best) best = src.chance;
    }
    return best;
}

float UnitAttributeAggregator::ComputeCritMultiplier() const {
    float bestChance = 0.0f;
    float multiplier = 1.0f;
    for (const auto& src : critSources_) {
        if (src.chance > bestChance) {
            bestChance = src.chance;
            multiplier = src.multiplier;
        }
    }
    return multiplier;
}

void UnitAttributeAggregator::SetCriticalStrike(float chance, float multiplier) {
    critSources_.clear();
    if (chance != 0.0f || multiplier != 1.0f) {
        critSources_.push_back({War3ID(0u), chance, multiplier});
    }
}

void UnitAttributeAggregator::AddAuraStatBonus(CAuraEffectType type, bool percent, War3ID buffId,
                                                float value, int caster,
                                                const CUnitType* unitType, int currentMaxMana) {
    auto key = std::make_pair((int)type, percent);
    auto& sources = auraStats_[key];
    for (auto& s : sources) {
        if (s.buffId == buffId && s.caster == caster) {
            s.value = value;
            RecomputeAuraStat(type, percent, unitType, currentMaxMana);
            return;
        }
    }
    sources.push_back({buffId, caster, value});
    RecomputeAuraStat(type, percent, unitType, currentMaxMana);
}

void UnitAttributeAggregator::RemoveAuraStatBonus(CAuraEffectType type, bool percent, War3ID buffId,
                                                   int caster, const CUnitType* unitType, int currentMaxMana) {
    auto key = std::make_pair((int)type, percent);
    auto it = auraStats_.find(key);
    if (it == auraStats_.end()) return;
    auto& sources = it->second;
    sources.erase(std::remove_if(sources.begin(), sources.end(),
                                 [&](const UnitAuraStatSource& s) {
                                     return s.buffId == buffId && s.caster == caster;
                                 }),
                  sources.end());
    if (sources.empty()) {
        auraStats_.erase(it);
    }
    RecomputeAuraStat(type, percent, unitType, currentMaxMana);
}

void UnitAttributeAggregator::RecomputeAllAuraStats(const CUnitType* unitType, int currentMaxMana) {
    std::vector<std::pair<int, bool>> keys;
    keys.reserve(auraStats_.size());
    for (const auto& [key, sources] : auraStats_) {
        (void)sources;
        keys.push_back(key);
    }
    for (const auto& [type, percent] : keys) {
        RecomputeAuraStat(static_cast<CAuraEffectType>(type), percent, unitType, currentMaxMana);
    }
}

void UnitAttributeAggregator::RecomputeAuraStat(CAuraEffectType type, bool percent,
                                                 const CUnitType* unitType, int currentMaxMana) {
    auto key = std::make_pair((int)type, percent);
    float best = 0.0f;
    bool have = false;
    auto sit = auraStats_.find(key);
    if (sit != auraStats_.end()) {
        for (const auto& s : sit->second) {
            if (!have || s.value > best) {
                best = s.value;
                have = true;
            }
        }
    }
    float applied = 0.0f;
    auto ait = auraApplied_.find(key);
    if (ait != auraApplied_.end()) applied = ait->second;

    switch (type) {
        case CAuraEffectType::ARMOR: {
            const int oldFlat = (int)std::lround(applied);
            const int newFlat = have ? (int)std::lround(best) : 0;
            if (oldFlat != 0) AddBonusArmor(-oldFlat);
            if (newFlat != 0) AddBonusArmor(newFlat);
            if (newFlat != 0) auraApplied_[key] = (float)newFlat;
            else auraApplied_.erase(key);
            break;
        }
        case CAuraEffectType::MOVE_SPEED: {
            if (percent) {
                const float newFactor = have ? best : 0.0f;
                auraMoveSpeedPercent_ = newFactor;
                if (have) auraApplied_[key] = best;
                else auraApplied_.erase(key);
            } else {
                const int oldFlat = (int)applied;
                const int newFlat = have ? (int)best : 0;
                if (oldFlat != 0) bonusSpeed_ -= oldFlat;
                if (newFlat != 0) bonusSpeed_ += newFlat;
                if (newFlat != 0) auraApplied_[key] = (float)newFlat;
                else auraApplied_.erase(key);
            }
            break;
        }
        case CAuraEffectType::ATTACK_SPEED: {
            std::unordered_map<War3ID, float> bestById;
            if (sit != auraStats_.end()) {
                for (const auto& s : sit->second) {
                    auto it = bestById.find(s.buffId);
                    if (it == bestById.end() || s.value > it->second) {
                        bestById[s.buffId] = s.value;
                    }
                }
            }
            for (auto it = auraSpeedKeys_.begin(); it != auraSpeedKeys_.end();) {
                if (bestById.find(*it) == bestById.end()) {
                    ClearNonStackingAttackSpeed(*it);
                    it = auraSpeedKeys_.erase(it);
                } else {
                    ++it;
                }
            }
            for (const auto& [id, value] : bestById) {
                SetNonStackingAttackSpeed(id, value);
                auraSpeedKeys_.insert(id);
            }
            break;
        }
        case CAuraEffectType::DAMAGE: {
            float base = 0.0f;
            if (percent && unitType) {
                const auto& attacks = unitType->GetAttacks();
                if (!attacks.empty()) {
                    base = (float)attacks[0].damageBase;
                    if (attacks[0].damageDice > 0 && attacks[0].damageSidesPerDie > 0) {
                        base += (float)attacks[0].damageDice *
                                (float)(attacks[0].damageSidesPerDie + 1) * 0.5f;
                    }
                }
            }
            std::unordered_map<War3ID, float> bestById;
            if (sit != auraStats_.end()) {
                for (const auto& s : sit->second) {
                    auto it = bestById.find(s.buffId);
                    if (it == bestById.end() || s.value > it->second) {
                        bestById[s.buffId] = s.value;
                    }
                }
            }
            auto& owned = percent ? auraDamagePctKeys_ : auraDamageFlatKeys_;
            for (auto it = owned.begin(); it != owned.end();) {
                if (bestById.find(*it) == bestById.end()) {
                    if (percent) SetNonStackingDamagePctBonus(*it, 0);
                    else SetNonStackingDamageBonus(*it, 0);
                    it = owned.erase(it);
                } else {
                    ++it;
                }
            }
            for (const auto& [id, value] : bestById) {
                const int flat = percent ? (int)std::lround(base * value)
                                         : (int)std::lround(value);
                if (percent) SetNonStackingDamagePctBonus(id, flat);
                else SetNonStackingDamageBonus(id, flat);
                owned.insert(id);
            }
            break;
        }
        case CAuraEffectType::LIFE_REGEN: {
            if (applied != 0.0f) AddBonusLifeRegen(-applied);
            if (have && best != 0.0f) {
                AddBonusLifeRegen(best);
                auraApplied_[key] = best;
            } else {
                auraApplied_.erase(key);
            }
            break;
        }
        case CAuraEffectType::MANA_REGEN: {
            const float oldFlat = applied;
            float newFlat = 0.0f;
            if (have && best != 0.0f) {
                newFlat = percent ? best * static_cast<float>(currentMaxMana) : best;
            }
            if (oldFlat != 0.0f) AddBonusManaRegen(-oldFlat);
            if (newFlat != 0.0f) {
                AddBonusManaRegen(newFlat);
                auraApplied_[key] = newFlat;
            } else {
                auraApplied_.erase(key);
            }
            break;
        }
        default:
            break;
    }
}
