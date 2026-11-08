#include "auras.h"
#include "ecs/buffs.h"

// ===== Devotion Aura =====
CAbilityDevotionAura::CAbilityDevotionAura(int handleId)
    : CAbilityAura(handleId, War3ID("AHad"), 900.0f, true, true, false,
                   {{CAuraEffectType::ARMOR, 1.5f, false}}) {}

// ===== Endurance Aura =====
CAbilityEnduranceAura::CAbilityEnduranceAura(int handleId)
    : CAbilityAura(handleId, War3ID("AOae"), 900.0f, true, true, false,
                   {{CAuraEffectType::MOVE_SPEED, 0.10f, true},
                    {CAuraEffectType::ATTACK_SPEED, 0.10f, true}}) {}

// ===== Unholy Aura =====
CAbilityUnholyAura::CAbilityUnholyAura(int handleId)
    : CAbilityAura(handleId, War3ID("AUau"), 900.0f, true, true, false,
                   {{CAuraEffectType::MOVE_SPEED, 0.10f, true},
                    {CAuraEffectType::LIFE_REGEN, 0.5f, false}}) {}

// ===== Thorns Aura =====
CAbilityThornsAura::CAbilityThornsAura(int handleId)
    : CAbilityAura(handleId, War3ID("AEah"), 900.0f, true, true, false) {
    SetAuraBuff(War3ID("BEah"), []() {
        return std::make_unique<CBuffThorns>(2.0f, 0.10f, War3ID("BEah"));
    });
}

// ===== Brilliance Aura =====
CAbilityBrillianceAura::CAbilityBrillianceAura(int handleId)
    : CAbilityAura(handleId, War3ID("AHab"), 900.0f, true, true, false,
                   {{CAuraEffectType::MANA_REGEN, 0.75f, false}}) {}

// ===== Trueshot Aura =====
CAbilityTrueshotAura::CAbilityTrueshotAura(int handleId)
    : CAbilityAura(handleId, War3ID("AEar"), 900.0f, true, true, false,
                   {{CAuraEffectType::DAMAGE, 0.10f, true}}) {}

// ===== Vampiric Aura =====
CAbilityVampiricAura::CAbilityVampiricAura(int handleId)
    : CAbilityAura(handleId, War3ID("AUav"), 900.0f, true, true, false) {
    SetAuraBuff(War3ID("BUav"), []() {
        return std::make_unique<CBuffLifesteal>(2.0f, 0.15f);
    });
}

// ===== Command Aura =====
CAbilityCommandAura::CAbilityCommandAura(int handleId)
    : CAbilityAura(handleId, War3ID("AOac"), 900.0f, true, false, false,
                   {{CAuraEffectType::DAMAGE, 0.10f, true}}) {}

// ===== Regeneration Aura =====
CAbilityRegenerationAura::CAbilityRegenerationAura(int handleId)
    : CAbilityAura(handleId, War3ID("Aoar"), 900.0f, true, true, false,
                   {{CAuraEffectType::LIFE_REGEN, 1.0f, false}}) {}

// ===== Mana Regeneration Aura =====
CAbilityManaRegenerationAura::CAbilityManaRegenerationAura(int handleId)
    : CAbilityAura(handleId, War3ID("Aarm"), 900.0f, true, true, false,
                   {{CAuraEffectType::MANA_REGEN, 1.0f, false}}) {}

// ===== Disease Cloud =====
CAbilityDiseaseCloud::CAbilityDiseaseCloud(int handleId)
    : CAbilityAura(handleId, War3ID("Aapl"), 176.0f, false, false, true) {
    SetAuraBuff(War3ID("Bapl"), []() {
        return std::make_unique<CBuffDisease>(War3ID("Bapl"), 10.0f, 10.0f);
    });
}
