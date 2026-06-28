#include "unit_type.h"

CUnitType::CUnitType(
    const std::string& name, const std::string& legacyName, const std::string& iconPath, War3ID typeId,
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
    bool neutralBuildingShowMinimapIcon)
    : name_(name), legacyName_(legacyName), iconPath_(iconPath), typeId_(typeId)
    , maxLife_(maxLife), lifeRegen_(lifeRegen), manaRegen_(manaRegen), lifeRegenType_(lifeRegenType)
    , manaInitial_(manaInitial), manaMaximum_(manaMaximum), speed_(speed), defense_(defense)
    , defaultAutocastAbility_(defaultAutocastAbility), abilityList_(abilityList)
    , building_(building), movementType_(movementType)
    , defaultFlyingHeight_(defaultFlyingHeight), collisionSize_(collisionSize)
    , requiredAnimationTags_(requiredAnimationTags)
    , classifications_(classifications), attacks_(attacks), attacksEnabled_(attacksEnabled)
    , armorType_(armorType), raise_(raise), decay_(decay), defenseType_(defenseType)
    , impactZ_(impactZ), deathTime_(deathTime), targetedAs_(targetedAs)
    , defaultAcquisitionRange_(defaultAcquisitionRange), minimumAttackRange_(minimumAttackRange)
    , structuresBuilt_(structuresBuilt), unitsTrained_(unitsTrained)
    , researchesAvailable_(researchesAvailable), upgradesUsed_(upgradesUsed)
    , upgradeClassToType_(upgradeClassToType), upgradesTo_(upgradesTo)
    , itemsSold_(itemsSold), itemsMade_(itemsMade), unitRace_(unitRace)
    , goldCost_(goldCost), lumberCost_(lumberCost), foodUsed_(foodUsed), foodMade_(foodMade)
    , buildTime_(buildTime), goldRepairCost_(goldRepairCost), lumberRepairCost_(lumberRepairCost)
    , repairTime_(repairTime)
    , preventedPathingTypes_(preventedPathingTypes), requiredPathingTypes_(requiredPathingTypes)
    , propWindow_(propWindow), turnRate_(turnRate)
    , requirements_(requirements), requirementTiers_(requirementTiers)
    , level_(level), hero_(hero)
    , startingStrength_(startingStrength), strengthPerLevel_(strengthPerLevel)
    , startingAgility_(startingAgility), agilityPerLevel_(agilityPerLevel)
    , startingIntelligence_(startingIntelligence), intelligencePerLevel_(intelligencePerLevel)
    , primaryAttribute_(primaryAttribute)
    , heroAbilityList_(heroAbilityList), heroProperNames_(heroProperNames)
    , properNamesCount_(properNamesCount)
    , canFlee_(canFlee), priority_(priority)
    , revivesHeroes_(revivesHeroes), pointValue_(pointValue)
    , castBackswingPoint_(castBackswingPoint), castPoint_(castPoint)
    , canBeBuiltOnThem_(canBeBuiltOnThem), canBuildOnMe_(canBuildOnMe)
    , defenseUpgradeBonus_(defenseUpgradeBonus)
    , sightRadiusDay_(sightRadiusDay), sightRadiusNight_(sightRadiusNight)
    , extendedLineOfSight_(extendedLineOfSight)
    , goldBountyAwardedBase_(goldBountyAwardedBase), goldBountyAwardedDice_(goldBountyAwardedDice)
    , goldBountyAwardedSides_(goldBountyAwardedSides)
    , lumberBountyAwardedBase_(lumberBountyAwardedBase), lumberBountyAwardedDice_(lumberBountyAwardedDice)
    , lumberBountyAwardedSides_(lumberBountyAwardedSides)
    , neutralBuildingShowMinimapIcon_(neutralBuildingShowMinimapIcon) {}
