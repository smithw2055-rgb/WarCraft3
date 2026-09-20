#include "auras.h"
#include "ecs/buffs.h"
#include "ecs/csimulation.h"
#include "ecs/unit.h"

// ===== Devotion Aura =====
CAbilityDevotionAura::CAbilityDevotionAura(int handleId)
    : CAbilityAura(handleId, War3ID("AHad")) {
    SetRadius(900.0f);
    SetAffectsAllies(true);
    SetAffectsSelf(true);
    CAuraBonus armor;
    armor.type = CAuraEffectType::ARMOR;
    armor.value = 1.5f; // +1.5 armor per level
    AddBonus(armor);
}

// ===== Endurance Aura =====
CAbilityEnduranceAura::CAbilityEnduranceAura(int handleId)
    : CAbilityAura(handleId, War3ID("AOae")) {
    SetRadius(900.0f);
    SetAffectsAllies(true);
    SetAffectsSelf(true);
    CAuraBonus moveSpeed;
    moveSpeed.type = CAuraEffectType::MOVE_SPEED;
    moveSpeed.value = 0.10f; // +10% move speed
    moveSpeed.isPercent = true;
    AddBonus(moveSpeed);
    CAuraBonus attackSpeed;
    attackSpeed.type = CAuraEffectType::ATTACK_SPEED;
    attackSpeed.value = 0.10f; // +10% attack speed
    attackSpeed.isPercent = true;
    AddBonus(attackSpeed);
}

// ===== Unholy Aura =====
CAbilityUnholyAura::CAbilityUnholyAura(int handleId)
    : CAbilityAura(handleId, War3ID("AUau")) {
    SetRadius(900.0f);
    SetAffectsAllies(true);
    SetAffectsSelf(true);
    CAuraBonus moveSpeed;
    moveSpeed.type = CAuraEffectType::MOVE_SPEED;
    moveSpeed.value = 0.10f;
    moveSpeed.isPercent = true;
    AddBonus(moveSpeed);
    CAuraBonus lifeRegen;
    lifeRegen.type = CAuraEffectType::LIFE_REGEN;
    lifeRegen.value = 0.5f; // +0.5 HP/sec
    AddBonus(lifeRegen);
}

// ===== Thorns Aura =====
CAbilityThornsAura::CAbilityThornsAura(int handleId)
    : CAbilityAura(handleId, War3ID("AEah")) {
    SetRadius(900.0f);
    SetAffectsAllies(true);
    SetAffectsSelf(true);
    // Thorns reflects damage - handled in combat system
}

void CAbilityThornsAura::ApplyAura(CSimulation& game, CUnit* target) {
    if (!target) return;
    // Level-1 object data (DataA1=0.1): 10% melee reflect while in range.
    // The buff is refreshed every aura tick; RemoveAura takes it away.
    if (CBuff* existing = target->GetBuff(War3ID("BEah"))) {
        existing->RefreshDuration();
    } else {
        target->AddBuff(std::make_unique<CBuffThorns>(2.0f, 0.10f, War3ID("BEah")), game);
    }
}

void CAbilityThornsAura::RemoveAura(CSimulation& game, CUnit* target) {
    if (!target) return;
    target->RemoveBuff(War3ID("BEah"), game);
}

// ===== Brilliance Aura =====
CAbilityBrillianceAura::CAbilityBrillianceAura(int handleId)
    : CAbilityAura(handleId, War3ID("AHab")) {
    SetRadius(900.0f);
    SetAffectsAllies(true);
    SetAffectsSelf(true);
    CAuraBonus manaRegen;
    manaRegen.type = CAuraEffectType::MANA_REGEN;
    manaRegen.value = 0.75f; // +0.75 MP/sec
    AddBonus(manaRegen);
}

// ===== Trueshot Aura =====
CAbilityTrueshotAura::CAbilityTrueshotAura(int handleId)
    : CAbilityAura(handleId, War3ID("AEar")) {
    SetRadius(900.0f);
    SetAffectsAllies(true);
    SetAffectsSelf(true);
    CAuraBonus damage;
    damage.type = CAuraEffectType::DAMAGE;
    damage.value = 0.10f; // +10% ranged damage
    damage.isPercent = true;
    AddBonus(damage);
}

// ===== Vampiric Aura =====
CAbilityVampiricAura::CAbilityVampiricAura(int handleId)
    : CAbilityAura(handleId, War3ID("AUav")) {
    SetRadius(900.0f);
    SetAffectsAllies(true);
    SetAffectsSelf(true);
    // Life steal - handled in combat system
}

void CAbilityVampiricAura::ApplyAura(CSimulation& game, CUnit* target) {
    if (!target) return;
    // Level-1 object data (DataA1=0.15): 15% life steal while in range.
    if (CBuff* existing = target->GetBuff(War3ID("BUav"))) {
        existing->RefreshDuration();
    } else {
        target->AddBuff(std::make_unique<CBuffLifesteal>(2.0f, 0.15f), game);
    }
}

void CAbilityVampiricAura::RemoveAura(CSimulation& game, CUnit* target) {
    if (!target) return;
    target->RemoveBuff(War3ID("BUav"), game);
}

// ===== Command Aura =====
CAbilityCommandAura::CAbilityCommandAura(int handleId)
    : CAbilityAura(handleId, War3ID("AOac")) {
    SetRadius(900.0f);
    SetAffectsAllies(true);
    SetAffectsSelf(false); // doesn't affect self
    CAuraBonus damage;
    damage.type = CAuraEffectType::DAMAGE;
    damage.value = 0.10f; // +10% damage
    damage.isPercent = true;
    AddBonus(damage);
}

// ===== Disease Cloud =====
CAbilityDiseaseCloud::CAbilityDiseaseCloud(int handleId)
    : CAbilityAura(handleId, War3ID("Aapl")) {
    SetRadius(176.0f); // ref Area1
    SetEnemyOnly(true);
    SetAffectsSelf(false);
}

void CAbilityDiseaseCloud::ApplyAura(CSimulation& game, CUnit* target) {
    if (!target) return;
    // Level-1 object data (DataC 10 dps, ref) while in the cloud.
    if (CBuff* existing = target->GetBuff(War3ID("Bapl"))) {
        existing->RefreshDuration();
    } else {
        target->AddBuff(std::make_unique<CBuffDisease>(War3ID("Bapl"), 10.0f, 10.0f), game);
    }
}

void CAbilityDiseaseCloud::RemoveAura(CSimulation& game, CUnit* target) {
    if (!target) return;
    target->RemoveBuff(War3ID("Bapl"), game);
}
