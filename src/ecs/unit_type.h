#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <unordered_set>
#include "resource/slk.h"
#include "data/combat_types.h"
#include "ecs/unit_attack.h"
#include "map/path_tex.h"

enum class PathingGridMovementType : uint8_t {
    FOOT, FLY, HOVER, FLOAT, AMPHIBIOUS, DISABLED, FOOT_NO_COLLISION, HORSE
};

enum class CUnitClassification : uint8_t {
    GIANT, UNDEAD, SUMMONED, MECHANICAL, PEON, SAPPER, TOWNHALL,
    ANCIENT, TREE, WARD, STANDONBUILDING, GOLD_MINE, STATUE,
    WALKWAY, BRIDGE, FLYING, CANNOT_BUILD_ON_ME,
    // Retail UnitData.slk "type" keys (ref CUnitClassification).
    STANDON, NEUTRAL, TAUREN
};

inline bool ParseUnitClassification(const std::string& key, CUnitClassification& out) {
    if (key == "giant") { out = CUnitClassification::GIANT; return true; }
    if (key == "undead") { out = CUnitClassification::UNDEAD; return true; }
    if (key == "summoned") { out = CUnitClassification::SUMMONED; return true; }
    if (key == "mechanical") { out = CUnitClassification::MECHANICAL; return true; }
    if (key == "peon") { out = CUnitClassification::PEON; return true; }
    if (key == "sapper") { out = CUnitClassification::SAPPER; return true; }
    if (key == "townhall") { out = CUnitClassification::TOWNHALL; return true; }
    if (key == "ancient") { out = CUnitClassification::ANCIENT; return true; }
    if (key == "tree") { out = CUnitClassification::TREE; return true; }
    if (key == "ward") { out = CUnitClassification::WARD; return true; }
    if (key == "standon") { out = CUnitClassification::STANDON; return true; }
    if (key == "neutral") { out = CUnitClassification::NEUTRAL; return true; }
    if (key == "tauren") { out = CUnitClassification::TAUREN; return true; }
    return false;
}

enum class CPrimaryAttribute : uint8_t {
    STRENGTH, AGILITY, INTELLIGENCE
};

struct CUnitTypeRequirement {
    War3ID id;
    int level = 0;
};

enum class CBuildingPathingType : uint8_t {
    ANY, BLIGHTED, UNFIXABLE, UNBUILDABLE, UNWALKABLE, UNFLYABLE
};

struct CUnitRace {
    std::string name;
    War3ID id;
};

// ============================================================
// CUnitType - static data for a unit type
// ============================================================
class CUnitType {
public:
    CUnitType(
        const std::string& name,
        const std::string& legacyName,
        const std::string& iconPath,
        War3ID typeId,
        int maxLife, float lifeRegen, float manaRegen, CRegenType lifeRegenType,
        int manaInitial, int manaMaximum, int speed, int defense,
        War3ID defaultAutocastAbility,
        const std::vector<War3ID>& abilityList,
        bool building, PathingGridMovementType movementType,
        float defaultFlyingHeight, float collisionSize,
        const std::vector<std::string>& requiredAnimationTags,
        const std::unordered_set<CUnitClassification>& classifications,
        const std::vector<CUnitAttack>& attacks,
        int attacksEnabled, const std::string& armorType,
        bool raise, bool decay, CDefenseType defenseType,
        float impactZ, float deathTime,
        const std::unordered_set<CTargetType>& targetedAs,
        float defaultAcquisitionRange, float minimumAttackRange,
        const std::vector<War3ID>& structuresBuilt,
        const std::vector<War3ID>& unitsTrained,
        const std::vector<War3ID>& researchesAvailable,
        const std::vector<War3ID>& upgradesUsed,
        const std::unordered_map<CUpgradeClass, War3ID>& upgradeClassToType,
        const std::vector<War3ID>& upgradesTo,
        const std::vector<War3ID>& itemsSold,
        const std::vector<War3ID>& itemsMade,
        const CUnitRace& unitRace,
        int goldCost, int lumberCost, int foodUsed, int foodMade,
        int buildTime, int goldRepairCost, int lumberRepairCost, int repairTime,
        const std::unordered_set<CBuildingPathingType>& preventedPathingTypes,
        const std::unordered_set<CBuildingPathingType>& requiredPathingTypes,
        float propWindow, float turnRate,
        const std::vector<CUnitTypeRequirement>& requirements,
        const std::vector<std::vector<CUnitTypeRequirement>>& requirementTiers,
        int level, bool hero,
        int startingStrength, float strengthPerLevel,
        int startingAgility, float agilityPerLevel,
        int startingIntelligence, float intelligencePerLevel,
        CPrimaryAttribute primaryAttribute,
        const std::vector<War3ID>& heroAbilityList,
        const std::vector<std::string>& heroProperNames,
        int properNamesCount,
        bool canFlee, int priority,
        bool revivesHeroes, int pointValue,
        float castBackswingPoint, float castPoint,
        bool canBeBuiltOnThem, bool canBuildOnMe,
        int defenseUpgradeBonus,
        int sightRadiusDay, int sightRadiusNight, bool extendedLineOfSight,
        int goldBountyAwardedBase, int goldBountyAwardedDice, int goldBountyAwardedSides,
        int lumberBountyAwardedBase, int lumberBountyAwardedDice, int lumberBountyAwardedSides,
        bool neutralBuildingShowMinimapIcon);

    const std::string& GetName() const { return name_; }
    const std::string& GetLegacyName() const { return legacyName_; }
    const std::string& GetIconPath() const { return iconPath_; }
    War3ID GetTypeId() const { return typeId_; }

    int GetMaxLife() const { return maxLife_; }
    float GetLifeRegen() const { return lifeRegen_; }
    float GetManaRegen() const { return manaRegen_; }
    CRegenType GetLifeRegenType() const { return lifeRegenType_; }
    int GetManaInitial() const { return manaInitial_; }
    int GetManaMaximum() const { return manaMaximum_; }

    int GetSpeed() const { return speed_; }
    int GetDefense() const { return defense_; }
    int GetPointValue() const { return pointValue_; }
    void SetPointValue(int v) { pointValue_ = v; }
    int GetDefenseUpgradeBonus() const { return defenseUpgradeBonus_; }
    War3ID GetDefaultAutocastAbility() const { return defaultAutocastAbility_; }
    const std::vector<War3ID>& GetAbilityList() const { return abilityList_; }
    bool IsBuilding() const { return building_; }
    PathingGridMovementType GetMovementType() const { return movementType_; }
    float GetDefaultFlyingHeight() const { return defaultFlyingHeight_; }
    float GetCollisionSize() const { return collisionSize_; }
    const std::vector<std::string>& GetRequiredAnimationTags() const { return requiredAnimationTags_; }
    const std::unordered_set<CUnitClassification>& GetClassifications() const { return classifications_; }
    const std::vector<CUnitAttack>& GetAttacks() const { return attacks_; }
    int GetAttacksEnabled() const { return attacksEnabled_; }
    const std::string& GetArmorType() const { return armorType_; }
    bool CanRaise() const { return raise_; }
    const std::unordered_set<CBuildingPathingType>& GetPreventedPathingTypes() const { return preventedPathingTypes_; }
    const std::unordered_set<CBuildingPathingType>& GetRequiredPathingTypes() const { return requiredPathingTypes_; }
    void SetPreventedPathingTypes(std::unordered_set<CBuildingPathingType> v) { preventedPathingTypes_ = std::move(v); }
    void SetRequiredPathingTypes(std::unordered_set<CBuildingPathingType> v) { requiredPathingTypes_ = std::move(v); }
    bool CanDecay() const { return decay_; }
    CDefenseType GetDefenseType() const { return defenseType_; }
    float GetImpactZ() const { return impactZ_; }
    float GetDeathTime() const { return deathTime_; }
    const std::unordered_set<CTargetType>& GetTargetedAs() const { return targetedAs_; }
    float GetDefaultAcquisitionRange() const { return defaultAcquisitionRange_; }
    float GetMinimumAttackRange() const { return minimumAttackRange_; }
    const std::vector<War3ID>& GetStructuresBuilt() const { return structuresBuilt_; }
    const std::vector<War3ID>& GetUnitsTrained() const { return unitsTrained_; }
    const std::vector<War3ID>& GetResearchesAvailable() const { return researchesAvailable_; }
    const std::vector<War3ID>& GetUpgradesUsed() const { return upgradesUsed_; }
    void SetUpgradesUsed(std::vector<War3ID> v) { upgradesUsed_ = std::move(v); }
    bool HasUpgradeClass(CUpgradeClass upgradeClass) const {
        return upgradeClassToType_.find(upgradeClass) != upgradeClassToType_.end();
    }
    const std::vector<War3ID>& GetUpgradesTo() const { return upgradesTo_; }
    const std::vector<War3ID>& GetItemsSold() const { return itemsSold_; }
    const CUnitRace& GetUnitRace() const { return unitRace_; }
    int GetGoldCost() const { return goldCost_; }
    int GetLumberCost() const { return lumberCost_; }
    int GetFoodUsed() const { return foodUsed_; }
    int GetFoodMade() const { return foodMade_; }
    int GetBuildTime() const { return buildTime_; }
    int GetGoldRepairCost() const { return goldRepairCost_; }
    int GetLumberRepairCost() const { return lumberRepairCost_; }
    int GetRepairTime() const { return repairTime_; }
    float GetPropWindow() const { return propWindow_; }
    float GetTurnRate() const { return turnRate_; }
    float GetCastBackswingPoint() const { return castBackswingPoint_; }
    float GetCastPoint() const { return castPoint_; }
    const std::vector<CUnitTypeRequirement>& GetRequirements() const { return requirements_; }
    // Hero tier lists, 1-based like ref getRequirementsTier (tier =
    // current hero count). Out-of-range tiers are empty (no requirement).
    const std::vector<CUnitTypeRequirement>& GetRequirementsTier(int tier) const {
        static const std::vector<CUnitTypeRequirement> kEmpty;
        const int index = tier - 1;
        if (index < 0 || index >= (int)requirementTiers_.size()) return kEmpty;
        return requirementTiers_[index];
    }
    int GetLevel() const { return level_; }
    bool IsHero() const { return hero_; }
    int GetStartingStrength() const { return startingStrength_; }
    float GetStrengthPerLevel() const { return strengthPerLevel_; }
    int GetStartingAgility() const { return startingAgility_; }
    float GetAgilityPerLevel() const { return agilityPerLevel_; }
    int GetStartingIntelligence() const { return startingIntelligence_; }
    float GetIntelligencePerLevel() const { return intelligencePerLevel_; }
    CPrimaryAttribute GetPrimaryAttribute() const { return primaryAttribute_; }
    const std::vector<std::string>& GetHeroProperNames() const { return heroProperNames_; }
    int GetSightRadiusDay() const { return sightRadiusDay_; }
    int GetSightRadiusNight() const { return sightRadiusNight_; }
    bool HasExtendedLineOfSight() const { return extendedLineOfSight_; }
    // Workers flee when attacked instead of returning fire (ref canFlee).
    bool CanFlee() const { return canFlee_; }
    int GetGoldBountyAwardedBase() const { return goldBountyAwardedBase_; }
    int GetGoldBountyAwardedDice() const { return goldBountyAwardedDice_; }
    int GetGoldBountyAwardedSides() const { return goldBountyAwardedSides_; }
    int GetLumberBountyAwardedBase() const { return lumberBountyAwardedBase_; }
    int GetLumberBountyAwardedDice() const { return lumberBountyAwardedDice_; }
    int GetLumberBountyAwardedSides() const { return lumberBountyAwardedSides_; }
    // Warsmash MeleeUIMinimap: neutral buildings with this flag draw the
    // special minimap icon instead of a plain team-color dot.
    bool IsNeutralBuildingShowMinimapIcon() const { return neutralBuildingShowMinimapIcon_; }
    // Response sound set name (UnitUI `unitSound`, e.g. "Footman"): resolved
    // against Units\UnitAckSounds.slk (`<name><What|Pissed|Yes|...>` rows)
    // by audio/UnitSoundset (ref UnitSoundset).
    const std::string& GetUnitSound() const { return unitSound_; }
    void SetUnitSound(const std::string& s) { unitSound_ = s; }
    // Pathing footprint texture (unit `pathTex`, e.g.
    // "PathTextures\8x8Default.tga"): decoded on demand for build
    // placement checks (ref buildingPathingPixelMap).
    const std::string& GetPathTex() const { return pathTex_; }
    void SetPathTex(const std::string& s) { pathTex_ = s; }
    std::shared_ptr<const PathTexImage> GetPathTexImage() const { return pathTexImage_; }
    void SetPathTexImage(std::shared_ptr<const PathTexImage> img) { pathTexImage_ = std::move(img); }

private:
    std::string name_;
    std::string legacyName_;
    std::string iconPath_;
    War3ID typeId_;
    int maxLife_;
    float lifeRegen_;
    float manaRegen_;
    CRegenType lifeRegenType_;
    int manaInitial_;
    int manaMaximum_;
    int speed_;
    int defense_;
    War3ID defaultAutocastAbility_;
    std::vector<War3ID> abilityList_;
    bool building_;
    PathingGridMovementType movementType_;
    float defaultFlyingHeight_;
    float collisionSize_;
    std::vector<std::string> requiredAnimationTags_;
    std::unordered_set<CUnitClassification> classifications_;
    std::vector<CUnitAttack> attacks_;
    int attacksEnabled_;
    std::string armorType_;
    bool raise_;
    bool decay_;
    CDefenseType defenseType_;
    float impactZ_;
    float deathTime_;
    std::unordered_set<CTargetType> targetedAs_;
    float defaultAcquisitionRange_;
    float minimumAttackRange_;
    std::vector<War3ID> structuresBuilt_;
    std::vector<War3ID> unitsTrained_;
    std::vector<War3ID> researchesAvailable_;
    std::vector<War3ID> upgradesUsed_;
    std::unordered_map<CUpgradeClass, War3ID> upgradeClassToType_;
    std::vector<War3ID> upgradesTo_;
    std::vector<War3ID> itemsSold_;
    std::vector<War3ID> itemsMade_;
    CUnitRace unitRace_;
    int goldCost_;
    int lumberCost_;
    int foodUsed_;
    int foodMade_;
    int buildTime_;
    int goldRepairCost_;
    int lumberRepairCost_;
    int repairTime_;
    std::unordered_set<CBuildingPathingType> preventedPathingTypes_;
    std::unordered_set<CBuildingPathingType> requiredPathingTypes_;
    float propWindow_;
    float turnRate_;
    std::vector<CUnitTypeRequirement> requirements_;
    std::vector<std::vector<CUnitTypeRequirement>> requirementTiers_;
    int level_;
    bool hero_;
    int startingStrength_;
    float strengthPerLevel_;
    int startingAgility_;
    float agilityPerLevel_;
    int startingIntelligence_;
    float intelligencePerLevel_;
    CPrimaryAttribute primaryAttribute_;
    std::vector<War3ID> heroAbilityList_;
    std::vector<std::string> heroProperNames_;
    int properNamesCount_;
    bool canFlee_;
    int priority_;
    bool revivesHeroes_;
    int pointValue_;
    float castBackswingPoint_;
    float castPoint_;
    bool canBeBuiltOnThem_;
    bool canBuildOnMe_;
    int defenseUpgradeBonus_;
    int sightRadiusDay_;
    int sightRadiusNight_;
    bool extendedLineOfSight_;
    int goldBountyAwardedBase_;
    int goldBountyAwardedDice_;
    int goldBountyAwardedSides_;
    int lumberBountyAwardedBase_;
    int lumberBountyAwardedDice_;
    int lumberBountyAwardedSides_;
    bool neutralBuildingShowMinimapIcon_;
    std::string unitSound_;
    std::string pathTex_;
    std::shared_ptr<const PathTexImage> pathTexImage_;
};
