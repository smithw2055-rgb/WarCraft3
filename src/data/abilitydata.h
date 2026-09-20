#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <array>
#include <unordered_map>
#include <memory>
#include "resource/slk.h"

// Full ability definition loaded from AbilityData.slk + AbilitySkin.txt etc.
struct CAbilityDefFull {
    War3ID id;
    std::string name;
    std::string legacyName;
    std::string tooltip;
    std::string uberTip;
    std::string hotkey;
    std::string iconArt;
    std::string effectArt;
    std::string targetsAllowed;
    std::string castType;
    std::string dataType;

    int buttonPosX = -1;
    int buttonPosY = -1;
    int manaCost = 0;
    float cooldown = 0;
    float castTime = 0;
    float duration = 0;
    float area = 0;
    float range = 0;
    int levels = 1;
    int race = 0;

    // Per-level combat fields (level 1 at index 0; retail repeats the
    // last level into higher blocks, so over-long vectors are normal).
    // Missing blocks fall back to the level-1 scalar above.
    std::vector<float> cooldowns;
    std::vector<int> manaCosts;
    std::vector<float> castTimes;
    std::vector<float> durations;
    std::vector<float> areas;
    std::vector<float> ranges;

    float CooldownAt(int level) const;
    int ManaCostAt(int level) const;
    float CastTimeAt(int level) const;
    float DurationAt(int level) const;
    float AreaAt(int level) const;
    float RangeAt(int level) const;

    // Effect/sound art per effecttype ordinal (ref ConvertEffectType 0..6:
    // EFFECT/TARGET/CASTER/SPECIAL/AREA_EFFECT/MISSILE/LIGHTNING), from the
    // ability Func profile skin tables. Empty when the profile has none.
    std::vector<std::string> effectArtByType = std::vector<std::string>(7);
    std::string effectSound;

    std::vector<std::string> dataFields; // raw data fields A-I

    // Per-level raw data fields A-I (level 1 at index 0).
    std::vector<std::array<std::string, 9>> dataLevels;

    std::string DataAt(int level, int field) const;
    float DataFloatAt(int level, int field, float fallback) const;
    int DataIntAt(int level, int field, int fallback) const;
};

struct CBuffDefFull {
    War3ID id;
    std::string name;
    std::string tooltip;
    std::string uberTip;
    std::string iconArt;
};

// Ability-to-unit mappings from UnitAbilities.slk
struct CUnitAbilityEntry {
    War3ID unitId;
    War3ID abilityId;
    int level = 1;
    bool heroAbility = false;
    bool autocast = false;
};

class CAbilityData {
public:
    CAbilityData() = default;

    bool HasAbility(War3ID id) const;
    const CAbilityDefFull* GetAbility(War3ID id) const;
    const CBuffDefFull* GetBuff(War3ID id) const;

    using AbilityMap = std::unordered_map<War3ID, std::unique_ptr<CAbilityDefFull>>;
    using BuffMap = std::unordered_map<War3ID, std::unique_ptr<CBuffDefFull>>;
    const AbilityMap& GetAll() const { return abilities_; }
    const BuffMap& GetAllBuffs() const { return buffs_; }

    static CAbilityData LoadFromMpq(const class MpqManager& mpq);
    static CAbilityData LoadFromMpq(const class MpqManager& mpq, const std::string& mapPath);

private:
    AbilityMap abilities_;
    BuffMap buffs_;
};
