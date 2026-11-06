#pragma once
#include <cstdint>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include "resource/slk.h"
#include "ecs/abilities/ability_aura.h"

class CUnitType;

// Evasion / critical strike sources (ref evasionListeners + crit listeners:
// every source rolls independently). Same-id installs replace; removals only
// clear their own id.
struct UnitEvasionSource {
    War3ID id = War3ID(0u);
    float chance = 0.0f;
};

struct UnitCritSource {
    War3ID id = War3ID(0u);
    float chance = 0.0f;
    float multiplier = 1.0f;
};

struct UnitAuraStatSource {
    War3ID buffId;
    int caster = -1;
    float value = 0.0f;
};

// ============================================================
// UnitAttributeAggregator - Unity GAS / Godot Resource style
// attribute & modifier aggregation pipeline for CUnit.
//
// Centralizes all layered stats (base + hero + bonus),
// non-stacking keyed buff tables, stacking item tables,
// aura stat slot synchronization, and independent proc sources
// (evasion & critical strike).
// ============================================================
class UnitAttributeAggregator {
public:
    UnitAttributeAggregator() = default;

    // --- Initialization from CUnitType ---
    void InitFromUnitType(const CUnitType* unitType);

    // --- Movement Speed ---
    int GetBaseSpeed() const { return baseSpeed_; }
    void SetBaseSpeed(int s) { baseSpeed_ = s; }
    int GetBonusSpeed() const { return bonusSpeed_; }
    void AddBonusSpeed(int v) { bonusSpeed_ += v; }
    void AddSpeedModifier(War3ID id, float percent) { speedModifiers_[id] = percent; }
    void RemoveSpeedModifier(War3ID id) { speedModifiers_.erase(id); }
    float ComputeEffectiveSpeed() const;

    // --- Vitality (Max Life / Max Mana) & Regen Layers ---
    int GetBaseMaxLife() const { return baseMaxLife_; }
    void SetBaseMaxLife(int v) { baseMaxLife_ = v; }
    float GetBonusMaxLife() const { return bonusMaxLife_; }
    void AddBonusMaxLife(float v) { bonusMaxLife_ += v; }
    float GetHeroBonusLife() const { return heroBonusLife_; }
    void SetHeroBonusLife(float v) { heroBonusLife_ = v; }
    float ComputeMaxLife() const;

    int GetBaseMaxMana() const { return baseMaxMana_; }
    void SetBaseMaxMana(int v) { baseMaxMana_ = v; }
    int GetBonusMaxMana() const { return bonusMaxMana_; }
    void AddBonusMaxMana(int v) { bonusMaxMana_ += v; }
    int GetHeroBonusMana() const { return heroBonusMana_; }
    void SetHeroBonusMana(int v) { heroBonusMana_ = v; }
    int ComputeMaxMana() const;

    float GetBonusLifeRegen() const { return bonusLifeRegen_; }
    void AddBonusLifeRegen(float v) { bonusLifeRegen_ += v; }
    float GetBonusManaRegen() const { return bonusManaRegen_; }
    void AddBonusManaRegen(float v) { bonusManaRegen_ += v; }

    // --- Defense Layers ---
    int GetCurrentDefense() const { return currentDefense_; }
    void SetCurrentDefense(int v) { currentDefense_ = v; }
    int GetBonusArmor() const { return bonusArmor_; }
    void AddBonusArmor(int v) { bonusArmor_ += v; }
    float GetTempDefenseBonus() const { return tempDefenseBonus_; }
    void SetTempDefenseBonus(float v) { tempDefenseBonus_ = v; }
    void AddTempDefenseBonus(float v) { tempDefenseBonus_ += v; }
    int ComputeTotalDefense() const { return currentDefense_ + bonusArmor_ + (int)tempDefenseBonus_; }

    // --- Damage Layers (Stacking + Non-Stacking Keyed Tables) ---
    void AddBonusDamage(int v) { bonusDamage_ += v; }
    int GetDirectBonusDamage() const { return bonusDamage_; }
    void SetHeroBonusDamage(int v) { heroBonusDamage_ = v; }
    int GetHeroBonusDamage() const { return heroBonusDamage_; }

    void SetNonStackingDamageBonus(War3ID key, int flatValue);
    void SetNonStackingDamagePctBonus(War3ID key, int flatValue);
    void ClearNonStackingDamageBonus(War3ID key);
    int GetNonStackingDamageTotal() const;
    float ComputeTotalBonusDamage() const {
        return (float)(bonusDamage_ + heroBonusDamage_ + GetNonStackingDamageTotal());
    }

    // --- Attack Speed & Range Layers ---
    void MultiplyAttackSpeed(float m) { attackSpeedMul_ *= m; }
    void DivideAttackSpeed(float m) { if (m != 0.0f) attackSpeedMul_ /= m; }
    void AddStackingAttackSpeed(float bonus) { attackSpeedStackBonus_ += bonus; }
    float GetStackingAttackSpeed() const { return attackSpeedStackBonus_; }

    void SetNonStackingAttackSpeed(War3ID key, float bonus);
    void ClearNonStackingAttackSpeed(War3ID key);
    float GetNonStackingAttackSpeedTotal() const;
    float ComputeAttackSpeedMultiplier() const;

    float GetAttackRangeBonus(int index) const;
    void AddAttackRangeBonus(int index, float v);
    void AddAttackRangeBonusAll(float v, size_t attackCount);

    // --- Evasion & Critical Strike Proc Aggregators ---
    void AddEvasionSource(War3ID id, float chance);
    void RemoveEvasionSource(War3ID id);
    const std::vector<UnitEvasionSource>& GetEvasionSources() const { return evasionSources_; }
    float ComputeEvasionChance() const;
    void SetEvasionChance(float v);

    void AddCritSource(War3ID id, float chance, float multiplier);
    void RemoveCritSource(War3ID id);
    const std::vector<UnitCritSource>& GetCritSources() const { return critSources_; }
    float ComputeCritChance() const;
    float ComputeCritMultiplier() const;
    void SetCriticalStrike(float chance, float multiplier);

    // --- Aura Stat Aggregation ---
    void AddAuraStatBonus(CAuraEffectType type, bool percent, War3ID buffId, float value,
                          int caster, const CUnitType* unitType, int currentMaxMana);
    void RemoveAuraStatBonus(CAuraEffectType type, bool percent, War3ID buffId,
                             int caster, const CUnitType* unitType, int currentMaxMana);
    void RecomputeAuraStat(CAuraEffectType type, bool percent,
                           const CUnitType* unitType, int currentMaxMana);
    void RecomputeAllAuraStats(const CUnitType* unitType, int currentMaxMana);

private:
    int baseSpeed_ = 0;
    int bonusSpeed_ = 0;
    float auraMoveSpeedPercent_ = 0.0f;
    std::unordered_map<War3ID, float> speedModifiers_;

    int baseMaxLife_ = 100;
    float bonusMaxLife_ = 0.0f;
    float heroBonusLife_ = 0.0f;

    int baseMaxMana_ = 0;
    int bonusMaxMana_ = 0;
    int heroBonusMana_ = 0;

    float bonusLifeRegen_ = 0.0f;
    float bonusManaRegen_ = 0.0f;

    int currentDefense_ = 0;
    int bonusArmor_ = 0;
    float tempDefenseBonus_ = 0.0f;

    int bonusDamage_ = 0;
    int heroBonusDamage_ = 0;
    std::unordered_map<War3ID, int> nsDamageFlat_;
    std::unordered_map<War3ID, int> nsDamagePct_;
    std::unordered_set<War3ID> auraDamageFlatKeys_;
    std::unordered_set<War3ID> auraDamagePctKeys_;

    float attackSpeedMul_ = 1.0f;
    float attackSpeedStackBonus_ = 0.0f;
    std::unordered_map<War3ID, float> nsAtkSpeed_;
    std::unordered_set<War3ID> auraSpeedKeys_;
    std::vector<float> attackRangeBonus_;

    std::vector<UnitEvasionSource> evasionSources_;
    std::vector<UnitCritSource> critSources_;

    std::map<std::pair<int, bool>, std::vector<UnitAuraStatSource>> auraStats_;
    std::map<std::pair<int, bool>, float> auraApplied_;
};
