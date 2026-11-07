#include "game/war3/war3_game_module.h"

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

#include <cmath>
#include "data/destructabledata.h"
#include "data/itemdata.h"
#include "data/unitdata.h"
#include "data/upgradedata.h"
#include "ecs/abilities/ability_attack.h"
#include "ecs/abilities/ability_build.h"
#include "ecs/abilities/ability_builder.h"
#include "ecs/abilities/ability_generic.h"
#include "ecs/abilities/ability_gold_mine.h"
#include "ecs/abilities/ability_harvest.h"
#include "ecs/abilities/ability_hero.h"
#include "ecs/abilities/ability_inventory.h"
#include "ecs/abilities/ability_move.h"
#include "ecs/abilities/ability_queue.h"
#include "ecs/abilities/ability_rally.h"
#include "ecs/abilities/ability_repair.h"
#include "ecs/abilities/ability_return_resources.h"
#include "ecs/abilities/ability_revive_hero.h"
#include "ecs/abilities/ability_root.h"
#include "ecs/abilities/ability_sell_items.h"
#include "ecs/abilities/ability_spell_base.h"
#include "ecs/abilities/ability_transport.h"
#include "ecs/abilities/ability_upgrade.h"
#include "ecs/abilities/auras.h"
#include "ecs/abilities/spells.h"
#include "ecs/abilities/spells2.h"
#include "ecs/abilities/spells3.h"
#include "ecs/abilities/spells4.h"
#include "ecs/abilities/spells5.h"
#include "ecs/abilities/spells6.h"
#include "ecs/abilities/spells7.h"
#include "ecs/abilities/spells8.h"
#include "ecs/abilities/spells9.h"
#include "ecs/abilities/spells10.h"
#include "ecs/abilities/spells11.h"
#include "ecs/abilities/spells12.h"
#include "ecs/abilities/spells13.h"
#include "ecs/abilities/spells14.h"
#include "ecs/abilities/spells15.h"
#include "ecs/abilities/spells16.h"
#include "ecs/abilities/spells17.h"
#include "ecs/abilities/spells18.h"
#include "ecs/abilities/spells19.h"
#include "ecs/abilities/spells20.h"
#include "ecs/abilities/spells21.h"
#include "ecs/abilities/spells22.h"
#include "ecs/abilities/spells23.h"
#include "ecs/abilities/spells24.h"
#include "ecs/abilities/spells25.h"
#include "ecs/abilities/spells26.h"
#include "ecs/abilities/spells27.h"
#include "ecs/abilities/spells28.h"
#include "ecs/abilities/spells29.h"
#include "ecs/ability.h"
#include "ecs/ability_factory.h"
#include "ecs/csimulation.h"
#include "ecs/destructable.h"
#include "ecs/item.h"
#include "ecs/player.h"
#include "ecs/unit.h"

namespace war3 {

namespace {

std::optional<CBuildRace> BuildRaceFromUnitType(const CUnitType& unitType) {
    const std::string& raceName = unitType.GetUnitRace().name;
    if (raceName == "human") {
        return CBuildRace::HUMAN;
    }
    if (raceName == "orc") {
        return CBuildRace::ORC;
    }
    if (raceName == "undead") {
        return CBuildRace::UNDEAD;
    }
    if (raceName == "nightelf" || raceName == "night elf") {
        return CBuildRace::NIGHT_ELF;
    }
    return std::nullopt;
}

War3ID BuildRaceAbilityCode(CBuildRace race) {
    switch (race) {
    case CBuildRace::ORC:
        return War3ID("AObu");
    case CBuildRace::UNDEAD:
        return War3ID("AUbu");
    case CBuildRace::NIGHT_ELF:
        return War3ID("AEbu");
    case CBuildRace::HUMAN:
    default:
        return War3ID("AHbu");
    }
}

} // namespace

War3GameModule::War3GameModule()
    : combatTable_(GetCanonicalWar3AttackDefenseTable()) {
    EnsureRegisteredInDynamicLoader();
}

std::shared_ptr<War3GameModule> War3GameModule::GetSharedDefaultInstance() {
    static std::shared_ptr<War3GameModule> s_default = std::make_shared<War3GameModule>();
    return s_default;
}

void War3GameModule::EnsureRegisteredInDynamicLoader() {
    static bool s_registered = false;
    if (!s_registered) {
        s_registered = true;
        engine::DynamicModuleLoader::Instance().RegisterBuiltInModule(
            kModuleId,
            []() -> std::shared_ptr<engine::ISimulationModule> {
                return std::make_shared<War3GameModule>();
            },
            []() -> std::unique_ptr<engine::IGameModule> {
                return std::make_unique<War3GameModule>();
            });
    }
}

void War3GameModule::RegisterWar3AbilityCatalog() {
    const std::string modId = kModuleId;

    auto regSimple = [&modId](const char* raw, auto makeFn) {
        AbilityFactory::RegisterModuleCreator(
            modId, War3ID(raw), [makeFn](int h, War3ID) -> std::unique_ptr<CAbility> {
                return makeFn(h);
            });
    };
    auto regAliased = [&modId](const char* raw, auto makeFn) {
        AbilityFactory::RegisterModuleCreator(
            modId, War3ID(raw), [makeFn](int h, War3ID code) -> std::unique_ptr<CAbility> {
                auto ab = makeFn(h);
                if (ab) ab->SetAlias(code);
                return ab;
            });
    };
    auto regWithCode = [&modId](const char* raw, auto makeFn) {
        AbilityFactory::RegisterModuleCreator(
            modId, War3ID(raw), [makeFn](int h, War3ID code) -> std::unique_ptr<CAbility> {
                return makeFn(h, code);
            });
    };

    #define REG_T(CodeStr, Cls) regSimple(CodeStr, [](int h) { return std::make_unique<Cls>(h); })
    #define REG_ALIAS(CodeStr, Cls) regAliased(CodeStr, [](int h) { return std::make_unique<Cls>(h); })

    // Core, Hero, Inventory
    REG_T("Aatk", CAbilityAttack);
    REG_T("Amov", CAbilityMove);
    REG_T("AHer", CAbilityHero);
    REG_T("AInv", CAbilityInventory);

    // Build abilities
    regWithCode("AHbu", [](int h, War3ID c) { return std::make_unique<CAbilityBuild>(h, c, CBuildRace::HUMAN); });
    regWithCode("AObu", [](int h, War3ID c) { return std::make_unique<CAbilityBuild>(h, c, CBuildRace::ORC); });
    regWithCode("AUbu", [](int h, War3ID c) { return std::make_unique<CAbilityBuild>(h, c, CBuildRace::UNDEAD); });
    regWithCode("AEbu", [](int h, War3ID c) { return std::make_unique<CAbilityBuild>(h, c, CBuildRace::NIGHT_ELF); });
    regWithCode("AGbu", [](int h, War3ID c) { return std::make_unique<CAbilityBuild>(h, c, CBuildRace::NAGA); });
    regWithCode("ANbu", [](int h, War3ID c) { return std::make_unique<CAbilityBuild>(h, c, CBuildRace::NEUTRAL); });

    // Harvest / Return / Repair / Economy / Queue / Transport / Root / Shop
    REG_T("Ahar", CAbilityHarvest);
    REG_T("Aret", CAbilityReturnResources);
    REG_ALIAS("Artn", CAbilityReturnResources);
    REG_ALIAS("Arep", CAbilityRepair);
    REG_ALIAS("Ahrp", CAbilityRepair);
    REG_ALIAS("Aren", CAbilityRepair);
    REG_ALIAS("Arst", CAbilityRepair);
    REG_T("Agld", CAbilityGoldMine);
    REG_T("Aque", CAbilityQueue);
    REG_T("ARal", CAbilityRally);
    REG_T("Aupg", CAbilityUpgrade);
    REG_T("Aloa", CAbilityLoad);
    REG_T("Adro", CAbilityUnload);
    REG_T("Aroo", CAbilityRoot);
    REG_T("Aro1", CAbilityUproot);
    REG_T("Asei", CAbilitySellItems);

    // Human / Orc / Undead / Night Elf / Neutral Spells (Batches 1-5)
    REG_T("AHbz", CAbilityBlizzard);
    REG_T("AHhb", CAbilityHolyLight);
    REG_T("AHds", CAbilityDivineShield);
    REG_T("AHtc", CAbilityThunderClap);
    REG_T("AHtb", CAbilityStormBolt);
    REG_ALIAS("ANfb", CAbilityStormBolt);
    REG_T("AOws", CAbilityWarStomp);
    REG_T("AOcl", CAbilityChainLightning);
    REG_T("AOhw", CAbilityHealingWave);
    REG_T("AOhx", CAbilityHex);
    REG_T("AOsf", CAbilityFeralSpirit);
    REG_T("AOeq", CAbilityEarthquake);
    REG_T("AUdc", CAbilityDeathCoil);
    REG_T("AUfn", CAbilityFrostNova);
    REG_T("AUcs", CAbilityCarrionSwarm);
    REG_T("AUsl", CAbilitySleep);
    REG_T("AUin", CAbilityInferno);
    REG_T("AEmb", CAbilityManaBurn);
    REG_T("AEer", CAbilityEntanglingRoots);
    REG_T("ANsi", CAbilitySilence);
    REG_T("ANba", CAbilityBlackArrow);
    REG_T("ANdr", CAbilityLifeDrain);
    REG_T("ANfd", CAbilityFingerOfDeath);
    REG_T("ANdp", CAbilityDarkPortal);
    REG_T("ANrc", CAbilityRainOfChaos);
    REG_T("AHmt", CAbilityMassTeleport);
    REG_T("AHwe", CAbilitySummonWaterElemental);
    REG_T("AHre", CAbilityResurrect);
    REG_T("AEbl", CAbilityBlink);
    REG_T("AOmi", CAbilityMirrorImage);
    REG_T("AOsh", CAbilityShockwave);
    REG_T("AEst", CAbilityOwlScout);
    REG_T("AEsf", CAbilityStarfall);
    REG_T("AEtq", CAbilityTranquility);
    REG_T("AOre", CAbilityReincarnation);
    regWithCode("ACrn", [](int h, War3ID c) { return std::make_unique<CAbilityReincarnation>(h, c); });
    regWithCode("AIrc", [](int h, War3ID c) { return std::make_unique<CAbilityReincarnation>(h, c, 0); });
    REG_T("ANch", CAbilityCharm);
    REG_ALIAS("AIco", CAbilityCharm);
    REG_T("AEev", CAbilityEvasion);
    REG_T("AOcr", CAbilityCriticalStrike);
    REG_T("AHbh", CAbilityBash);
    REG_T("Aslo", CAbilitySlow);
    REG_T("AUfa", CAbilityFrostArmor);
    REG_T("Afae", CAbilityFaerieFire);
    REG_T("AUdp", CAbilityDeathPact);
    REG_T("AUan", CAbilityAnimateDead);
    REG_T("ANdo", CAbilityDoom);
    REG_T("ANvc", CAbilityVolcano);
    REG_T("AHfs", CAbilityFlameStrike);
    REG_T("AUdd", CAbilityDeathAndDecay);
    REG_T("AUim", CAbilityImpale);
    REG_T("AEfk", CAbilityFanOfKnives);
    REG_T("ANrf", CAbilityRainOfFire);
    REG_T("AUts", CAbilitySpikedCarapace);
    REG_T("AUdr", CAbilityDarkRitual);
    REG_T("AEsh", CAbilityShadowStrike);

    // Batches 6-14
    REG_T("ANhs", CAbilityHealingSpray);
    REG_T("ANab", CAbilityAcidBomb);
    REG_T("ANcr", CAbilityChemicalRage);
    REG_T("ANtm", CAbilityTransmute);
    REG_T("Ablo", CAbilityBloodlustCast);
    REG_T("Aprg", CAbilityPurge);
    REG_T("Alsh", CAbilityLightningShield);
    REG_T("Arai", CAbilityRaiseDead);
    REG_ALIAS("AIrd", CAbilityRaiseDead);
    REG_T("AOsw", CAbilitySerpentWard);
    REG_T("Aply", CAbilityPolymorph);
    REG_T("Ainf", CAbilityInnerFire);
    REG_T("Ahea", CAbilityHeal);
    REG_T("Adis", CAbilityDispelMagic);
    REG_T("Arej", CAbilityRejuvenation);
    REG_ALIAS("AIrg", CAbilityRejuvenation);
    REG_T("Aroa", CAbilityRoar);
    REG_T("Adef", CAbilityDefend);
    REG_T("AIha", CAbilityItemHealGroup);
    REG_T("AItp", CAbilityItemTownPortal);
    REG_T("AIvl", CAbilityItemInvulnerable);
    REG_T("AIh1", CAbilityItemHealSelf);
    REG_T("AIm1", CAbilityItemManaSelf);
    REG_T("AIpl", CAbilityItemClarity);
    REG_T("Aste", CAbilityItemManaSteal);
    REG_T("AIda", CAbilityItemProtection);
    REG_T("AIob", CAbilityOrbOfFrost);
    REG_T("AIcb", CAbilityOrbOfCorruption);
    REG_T("AIfb", CAbilityOrbOfFire);
    REG_T("AIil", CAbilityIllusion);
    REG_T("AIls", CAbilityLightningShieldWand);
    REG_T("AIrr", CAbilityBeastScroll);
    REG_T("AIhw", CAbilityHealingWard);
    REG_T("ANca", CAbilityCleavingAttack);
    REG_T("AHfa", CAbilitySearingArrows);
    REG_T("Aens", CAbilityEnsnare);
    REG_T("Aast", CAbilityAncestralSpirit);
    REG_T("Ahwd", CAbilityHealingWardUnit);
    REG_T("Auco", CAbilityUnstableConcoction);
    REG_T("Aadm", CAbilityAbolishMagic);
    REG_T("ANcs", CAbilityClusterRockets);
    REG_T("AOwk", CAbilityWindWalk);
    REG_T("Aivs", CAbilityInvisibility);
    REG_T("Apiv", CAbilityPermanentInvisibility);
    REG_T("Ashm", CAbilityShadowmeld);
    REG_T("AItb", CAbilityDustOfAppearance);
    REG_T("Absk", CAbilityBerserk);
    REG_T("ANsg", CAbilitySummonBear);
    REG_T("ANbr", CAbilityBattleRoar);
    REG_T("Atru", CAbilityTrueSight);
    REG_ALIAS("Adet", CAbilityTrueSight);
    REG_ALIAS("Agyv", CAbilityTrueSight);
    REG_ALIAS("Adts", CAbilityTrueSight);
    REG_ALIAS("Abdt", CAbilityTrueSight);
    REG_T("Aeye", CAbilitySentryWard);
    REG_ALIAS("AIsw", CAbilitySentryWard);
    REG_T("Afla", CAbilityFlare);
    REG_T("Abur", CAbilityBurrow);
    REG_T("Acan", CAbilityCannibalize);
    REG_T("AUcb", CAbilityCarrionBeetles);
    REG_T("Aweb", CAbilityWeb);
    REG_T("AEme", CAbilityMetamorphosis);
    REG_T("Abrf", CAbilityBearForm);
    REG_T("Arav", CAbilityRavenForm);
    REG_T("ANrg", CAbilityRoboGoblin);
    REG_T("Astn", CAbilityStoneForm);
    REG_T("AHav", CAbilityAvatar);
    REG_T("AHdr", CAbilitySiphonMana);
    REG_T("AOvd", CAbilityBigBadVoodoo);
    REG_T("ANsy", CAbilityPocketFactory);
    REG_T("ANlm", CAbilityLavaSpawn);
    REG_T("ANsb", CAbilitySoulBurn);
    REG_T("ANsq", CAbilityQuilbeast);
    REG_T("Afsh", CAbilityFrostBreath);
    REG_T("Aapl", CAbilityDiseaseCloud);
    REG_T("Awar", CAbilityPulverize);
    REG_T("ANfl", CAbilityForkedLightning);

    // Batches 15-29
    REG_T("AHpx", CAbilityPhoenix);
    REG_T("Adev", CAbilityDevour);
    REG_T("Advm", CAbilityDevourMagic);
    REG_T("AHca", CAbilityColdArrows);
    REG_ALIAS("ANfa", CAbilityColdArrows);
    REG_T("ANmo", CAbilityMonsoon);
    REG_T("Aabs", CAbilityAbsorbMana);
    REG_T("Auhf", CAbilityUnholyFrenzy);
    REG_T("ANht", CAbilityHowlOfTerror);
    REG_T("ANbf", CAbilityBreathOfFire);
    REG_T("ANdh", CAbilityDrunkenHaze);
    REG_T("ANms", CAbilityManaShield);
    REG_T("AUls", CAbilityLocustSwarm);
    REG_T("Acyc", CAbilityCyclone);
    REG_T("Avng", CAbilityVengeance);
    REG_T("AOww", CAbilityWhirlwind);
    REG_T("AEim", CAbilityImmolation);
    REG_T("AEfn", CAbilityForceOfNature);
    REG_T("ANsw", CAbilitySummonHawk);
    REG_T("ANfy", CAbilityFactory);
    REG_T("Asds", CAbilityKaboom);
    REG_T("Apxf", CAbilityPhoenixFire);
    REG_T("AHbn", CAbilityBanish);
    REG_T("Aam2", CAbilityAntiMagicShell);
    REG_T("Apos", CAbilityPossession);
    REG_T("ANst", CAbilityStampede);
    REG_T("Asta", CAbilityStasisTrap);
    REG_T("ANto", CAbilityTornado);
    REG_T("Ache", CAbilityChainDispel);
    REG_T("ANta", CAbilityTaunt);
    REG_T("Aoar", CAbilityRegenerationAura);
    REG_ALIAS("Aabr", CAbilityRegenerationAura);
    REG_T("Aarm", CAbilityManaRegenerationAura);
    REG_T("AIas", CAbilityItemAttackSpeed);
    REG_T("AIrm", CAbilityItemManaRegen);
    REG_T("AIms", CAbilityItemMoveSpeed);
    REG_T("AIva", CAbilityItemVampirism);
    REG_ALIAS("Aasl", CAbilityEnduranceAura);
    REG_ALIAS("Aakb", CAbilityCommandAura);
    REG_T("ANdb", CAbilityDrunkenBrawler);
    REG_T("Acrs", CAbilityCurse);
    REG_T("Ambt", CAbilityMoonWell);
    REG_T("Aeat", CAbilityEatTree);
    REG_T("Asac", CAbilitySacrifice);
    REG_T("Alam", CAbilityRequestSacrifice);
    REG_T("Ahid", CAbilityHide);
    REG_T("Aeth", CAbilityEthereal);
    REG_T("AIgo", CAbilityChestOfGold);
    REG_T("AIat", CAbilityItemDamageBonus);
    REG_T("AIde", CAbilityItemArmorBonus);
    REG_T("AIml", CAbilityItemLifeBonus);
    REG_T("AImm", CAbilityItemManaPoolBonus);
    REG_T("AIab", CAbilityItemAttributeBonus);
    REG_T("ANss", CAbilitySpellShieldPassive);
    REG_T("ANse", CAbilitySpellShieldArea);
    regWithCode("AIim", [](int h, War3ID c) { return std::make_unique<CAbilityItemPermanentStats>(h, c, 0, 0, 1); });
    regWithCode("AIam", [](int h, War3ID c) { return std::make_unique<CAbilityItemPermanentStats>(h, c, 1, 0, 0); });
    regWithCode("AIxm", [](int h, War3ID c) { return std::make_unique<CAbilityItemPermanentStats>(h, c, 0, 1, 0); });
    regWithCode("AIsm", [](int h, War3ID c) { return std::make_unique<CAbilityItemPermanentStats>(h, c, 1, 1, 1); });
    REG_T("AImi", CAbilityItemPermanentLife);
    REG_T("AIem", CAbilityItemExperience);
    REG_T("AIlm", CAbilityItemLevelGain);
    REG_T("AIfs", CAbilityItemFigurine);
    REG_T("Avul", CAbilityPermanentInvulnerable);
    REG_T("AIct", CAbilityChangeTimeOfDay);
    regWithCode("Acpf", [](int h, War3ID c) { return std::make_unique<CAbilityToggleForm>(h, c); });
    regWithCode("Aetf", [](int h, War3ID c) { return std::make_unique<CAbilityToggleForm>(h, c); });
    regWithCode("Aave", [](int h, War3ID c) { return std::make_unique<CAbilityToggleForm>(h, c); });
    REG_T("Acha", CAbilityChaos);
    regWithCode("Aetl", [](int h, War3ID c) { return std::make_unique<CAbilityStatePassive>(h, c, CPassiveState::Ethereal); });
    regWithCode("Amim", [](int h, War3ID c) { return std::make_unique<CAbilityStatePassive>(h, c, CPassiveState::MagicImmune); });
    regWithCode("Arsk", [](int h, War3ID c) { return std::make_unique<CAbilityStatePassive>(h, c, CPassiveState::Resistant); });
    regWithCode("Agho", [](int h, War3ID c) { return std::make_unique<CAbilityStatePassive>(h, c, CPassiveState::Ghost); });
    REG_T("AIsr", CAbilityItemSpellResist);
    REG_T("AIdd", CAbilityItemMixedDefense);
    REG_T("Amdf", CAbilityMagicDefense);
    REG_T("Amls", CAbilityMagicLeash);
    regWithCode("Arpl", [](int h, War3ID c) { return std::make_unique<CAbilityReplenish>(h, c, true); });
    regWithCode("Arpm", [](int h, War3ID c) { return std::make_unique<CAbilityReplenish>(h, c, false); });
    REG_T("Asph", CAbilitySphere);
    regWithCode("Amic", [](int h, War3ID c) { return std::make_unique<CAbilityPairing>(h, c, CPairMode::MilitiaCall); });
    regWithCode("Amil", [](int h, War3ID c) { return std::make_unique<CAbilityPairing>(h, c, CPairMode::Militia); });
    regWithCode("Acoa", [](int h, War3ID c) { return std::make_unique<CAbilityPairing>(h, c, CPairMode::Mount); });
    regWithCode("Acoh", [](int h, War3ID c) { return std::make_unique<CAbilityPairing>(h, c, CPairMode::LoadPartner); });
    regWithCode("Acoi", [](int h, War3ID c) { return std::make_unique<CAbilityPairing>(h, c, CPairMode::Couple); });
    REG_T("Abli", CAbilityBlight);
    REG_T("Abgm", CAbilityBlightedGoldMine);
    REG_T("Agl2", CAbilityOverlayedGoldMine);
    REG_T("Aaha", CAbilityAcolyteHarvest);
    REG_T("Awha", CAbilityWispHarvest);
    REG_T("Ahrl", CAbilityHarvestLumber);
    regWithCode("Acar", [](int h, War3ID c) { return std::make_unique<CAbilityCargoHold>(h, c, false); });
    regWithCode("Abun", [](int h, War3ID c) { return std::make_unique<CAbilityCargoHold>(h, c, true); });
    regWithCode("Aenc", [](int h, War3ID c) { return std::make_unique<CAbilityCargoHold>(h, c, false, 5); });
    regWithCode("Adri", [](int h, War3ID c) { return std::make_unique<CAbilityDropInstant>(h, c); });
    regWithCode("Astd", [](int h, War3ID c) { return std::make_unique<CAbilityDropInstant>(h, c); });
    REG_T("Awrp", CAbilityWayGate);
    REG_T("Apit", CAbilityShopPurchase);
    regWithCode("Aneu", [](int h, War3ID c) { return std::make_unique<CAbilityNeutralBuilding>(h, c, false); });
    regWithCode("Aall", [](int h, War3ID c) { return std::make_unique<CAbilityNeutralBuilding>(h, c, true); });
    REG_T("Aent", CAbilityEntangleGoldMine);
    REG_T("Aegm", CAbilityEntangledMine);
    REG_T("ANcl", CAbilityChannelTest);

    // Auras
    REG_T("AHad", CAbilityDevotionAura);
    REG_T("AHab", CAbilityBrillianceAura);
    REG_T("AOae", CAbilityEnduranceAura);
    REG_T("AOac", CAbilityCommandAura);
    REG_T("AUau", CAbilityUnholyAura);
    REG_T("AUav", CAbilityVampiricAura);
    REG_T("AEah", CAbilityThornsAura);
    REG_T("AEar", CAbilityTrueshotAura);

    #undef REG_T
    #undef REG_ALIAS

    AbilityBuilderRegistry::Instance().RegisterStandardCanonicalDescriptors();
}

size_t War3GameModule::UnregisterWar3AbilityCatalog() {
    return AbilityFactory::UnregisterModuleCreators(kModuleId);
}

bool War3GameModule::IsRepairAbilityAlias(War3ID abilityCode) {
    return (abilityCode == War3ID("Arep")) || (abilityCode == War3ID("Ahrp")) ||
           (abilityCode == War3ID("Aren")) || (abilityCode == War3ID("Arst"));
}

bool War3GameModule::IsTownHallReturnResourceType(War3ID typeId) {
    return typeId == War3ID("htow") || typeId == War3ID("hkee") || typeId == War3ID("hcas") ||
           typeId == War3ID("ogre") || typeId == War3ID("ostr") || typeId == War3ID("ofrt") || typeId == War3ID("ofor") ||
           typeId == War3ID("unpl") || typeId == War3ID("unp1") || typeId == War3ID("unp2") ||
           typeId == War3ID("etol") || typeId == War3ID("etoa") || typeId == War3ID("etoe");
}

bool War3GameModule::IsStandardRaceWorkerType(War3ID typeId) {
    return typeId == War3ID("hpea") || typeId == War3ID("opeo") ||
           typeId == War3ID("uaco") || typeId == War3ID("ewsp");
}

bool War3GameModule::OnInit(engine::EngineKernel& kernel) {
    if (kernel.Input()) {
        kernel.Input()->RegisterStandardWarcraftDefaults();
    }
    OnRegisterAbilities();
    return true;
}

void War3GameModule::OnFixedTick(engine::EngineKernel& /*kernel*/, float fixedDt) {
    if (boundSim_) {
        TickPlayerAIs(*boundSim_, fixedDt);
        boundSim_->Update();
    }
}

void War3GameModule::OnFrameUpdate(engine::EngineKernel& /*kernel*/, float /*dt*/, float /*interpolationAlpha*/) {}

void War3GameModule::OnShutdown(engine::EngineKernel& /*kernel*/) {
    boundSim_ = nullptr;
}

void War3GameModule::OnAttachToSimulation(CSimulation& sim) {
    ISimulationModule::OnAttachToSimulation(sim);
    boundSim_ = &sim;
    OnRegisterAbilities();
}

void War3GameModule::OnDetachFromSimulation(CSimulation& sim) {
    ISimulationModule::OnDetachFromSimulation(sim);
    if (boundSim_ == &sim) {
        boundSim_ = nullptr;
    }
}

void War3GameModule::OnRegisterAbilities() {
    AbilityFactory::EnsureDefaultCatalogRegistered();
}

void War3GameModule::OnUnregisterAbilities() {
    UnregisterWar3AbilityCatalog();
}

void War3GameModule::OnAssembleUnitAbilities(CSimulation& sim,
                                             CUnit& unit,
                                             const CUnitType& unitType) {
    ++stats_.unitsAssembled;
    const War3ID typeId = unit.GetTypeId();
    const size_t abilitiesBefore = unit.GetAbilities().size();

    // 1. All War3 units receive Attack; movable units receive Move
    unit.AddAbility(sim, std::make_unique<CAbilityAttack>(sim.AllocateHandleId()));
    if (!unitType.IsBuilding()) {
        unit.AddAbility(sim, std::make_unique<CAbilityMove>(sim.AllocateHandleId()));
    }

    // 2. Heroes receive the Hero ability (attributes, XP, skill points)
    if (unitType.IsHero()) {
        unit.AddAbility(sim, std::make_unique<CAbilityHero>(sim.AllocateHandleId()));
    }

    // 3. Buildings receive Queue, Rally, Upgrade, Shop SellItems, GoldMine, and ReturnResources
    if (unitType.IsBuilding()) {
        unit.AddAbility(sim, std::make_unique<CAbilityQueue>(sim.AllocateHandleId()));
        unit.AddAbility(sim, std::make_unique<CAbilityRally>(sim.AllocateHandleId()));
        unit.AddAbility(sim, std::make_unique<CAbilityUpgrade>(sim.AllocateHandleId()));

        if (auto* queueAbility = dynamic_cast<CAbilityQueue*>(unit.GetAbility(War3ID("Aque")))) {
            for (const auto& trainId : unitType.GetUnitsTrained()) {
                queueAbility->AddTrainedUnit(trainId);
            }
            for (const auto& researchId : unitType.GetResearchesAvailable()) {
                queueAbility->AddResearch(researchId);
            }
        }
        if (auto* upgradeAbility = dynamic_cast<CAbilityUpgrade*>(unit.GetAbility(War3ID("Aupg")))) {
            for (const auto& upgradeId : unitType.GetUpgradesTo()) {
                upgradeAbility->AddUpgradeTo(upgradeId);
            }
        }

        if (!unitType.GetItemsSold().empty()) {
            unit.AddAbility(sim, std::make_unique<CAbilitySellItems>(
                sim.AllocateHandleId(), unitType.GetItemsSold()));
        }

        for (const auto& abilityCode : unitType.GetAbilityList()) {
            if (abilityCode == War3ID("Agld")) {
                unit.AddAbility(sim, std::make_unique<CAbilityGoldMine>(sim.AllocateHandleId()));
                break;
            }
        }

        bool hasReturnResources = IsTownHallReturnResourceType(typeId) ||
            (unitType.GetClassifications().count(CUnitClassification::TOWNHALL) > 0);
        for (const auto& abilityCode : unitType.GetAbilityList()) {
            if (abilityCode == War3ID("Aret")) {
                hasReturnResources = true;
                break;
            }
        }
        if (hasReturnResources) {
            unit.AddAbility(sim, std::make_unique<CAbilityReturnResources>(sim.AllocateHandleId()));
        }
    } else {
        // 4. Non-building workers receive Harvest, Race Build, and Repair
        for (const auto& abilityCode : unitType.GetAbilityList()) {
            if (abilityCode == War3ID("Ahar")) {
                unit.AddAbility(sim, std::make_unique<CAbilityHarvest>(sim.AllocateHandleId()));
                break;
            }
        }
        if (!unit.GetAbility(War3ID("Ahar")) && IsStandardRaceWorkerType(typeId)) {
            auto harvest = std::make_unique<CAbilityHarvest>(sim.AllocateHandleId());
            if (typeId == War3ID("uaco")) {
                harvest->SetLumberCapacity(0);
            }
            unit.AddAbility(sim, std::move(harvest));
        }

        if (!unitType.GetStructuresBuilt().empty() &&
            !unit.GetAbility(War3ID("AHbu")) && !unit.GetAbility(War3ID("AObu")) &&
            !unit.GetAbility(War3ID("AUbu")) && !unit.GetAbility(War3ID("AEbu"))) {
            if (const auto buildRace = BuildRaceFromUnitType(unitType)) {
                auto buildAbility = std::make_unique<CAbilityBuild>(
                    sim.AllocateHandleId(), BuildRaceAbilityCode(*buildRace), *buildRace);
                for (const auto& structureId : unitType.GetStructuresBuilt()) {
                    buildAbility->AddBuildableStructure(structureId);
                }
                unit.AddAbility(sim, std::move(buildAbility));
            }
        }

        for (const auto& abilityCode : unitType.GetAbilityList()) {
            if (IsRepairAbilityAlias(abilityCode)) {
                auto repairAbility = std::make_unique<CAbilityRepair>(sim.AllocateHandleId());
                repairAbility->SetAlias(abilityCode);
                unit.AddAbility(sim, std::move(repairAbility));
                break;
            }
        }
    }

    // 5. Units with AInv receive Inventory ability
    for (const auto& abilityCode : unitType.GetAbilityList()) {
        if (abilityCode == War3ID("AInv")) {
            unit.AddAbility(sim, std::make_unique<CAbilityInventory>(sim.AllocateHandleId()));
            break;
        }
    }

    // 6. Attach remaining abilities from unitType's abilityList via AbilityFactory
    AbilityFactory::AddDefaultAbilities(sim, &unit, unitType.GetAbilityList());

    const size_t abilitiesAfter = unit.GetAbilities().size();
    if (abilitiesAfter >= abilitiesBefore) {
        stats_.abilitiesAttached += (abilitiesAfter - abilitiesBefore);
    }
}

void War3GameModule::OnConfigureCreatedItem(CSimulation& sim, CItem& item) {
    ++stats_.itemsConfigured;
    const CItemData* itemData = sim.GetItemData();
    if (!itemData) return;
    const CItemType* itemType = itemData->GetItem(item.GetTypeId());
    if (!itemType) return;

    item.SetName(itemType->name);
    item.SetIcon(itemType->iconPath);
    item.SetAbilityList(itemType->abilityList);
    item.SetMaxLife(static_cast<float>(itemType->maxLife));
    item.SetLife(sim, static_cast<float>(itemType->maxLife));
    item.SetGoldValue(itemType->goldCost);
    item.SetLumberValue(itemType->lumberCost);
    item.SetItemClass(itemType->itemClass);
    item.SetCharges(itemType->charges);
    item.SetPerishable(itemType->perishable);
    item.SetPowerup(itemType->powerup);
    item.SetUseAutomatically(itemType->useOnly);
    item.SetDroppable(itemType->droppable);
    item.SetPawnable(itemType->pawnable);
    item.SetDroppedOnDeath(itemType->dropOnDeath);
    if (itemType->charges > 0) {
        item.SetCharged(true);
    }
}

void War3GameModule::OnConfigureCreatedDestructable(CSimulation& sim,
                                                     CDestructable& destructable,
                                                     float initialLife,
                                                     int /*rotationDeg*/) {
    ++stats_.destructablesConfigured;
    if (const CDestructableData* destructableData = sim.GetDestructableData()) {
        if (const auto* type = destructableData->GetDestructable(destructable.GetTypeId())) {
            destructable.SetMaxLife(static_cast<float>(type->maxLife));
            destructable.SetLife(sim, std::clamp(initialLife, 0.0f, static_cast<float>(type->maxLife)));
            destructable.SetOccluderHeight(type->occlusionHeight);
        }
    }
}

void War3GameModule::OnUnitCommittedToWorld(CSimulation& sim, CUnit& unit) {
    ++stats_.unitsCommitted;
    if (CPlayer* player = sim.GetPlayer(unit.GetPlayerIndex())) {
        player->AddUnit(&unit);
        if (unit.IsHero()) {
            player->AddHero(&unit);
        }
        player->AddTechUnlocked(sim, unit.GetTypeId());
        if (unit.GetFoodUsed() > 0) {
            player->SetFoodUsed(player->GetFoodUsed() + unit.GetFoodUsed());
        }
        if (unit.IsBuilding() && unit.GetFoodMade() > 0 && !unit.IsConstructing()) {
            player->AddFoodCap(unit.GetFoodMade());
        }
        // Apply any upgrades already researched by the owning player to the newly committed unit
        OnApplyPlayerUpgradesToUnit(sim, *player, unit);
    }
}

void War3GameModule::OnUnitDied(CSimulation& sim, CUnit& unit) {
    ++stats_.unitsDied;
    if (CPlayer* player = sim.GetPlayer(unit.GetPlayerIndex())) {
        // Non-heroes stop satisfying techtree requirements upon death (heroes retain possession while awaiting Altar/Tavern revival)
        if (!unit.IsHero()) {
            player->RemoveTechUnlocked(sim, unit.GetTypeId());
        }
        // Refund food cap & food used at death (and zero on the unit so subsequent removal does not double-refund)
        const int made = unit.GetFoodMade();
        if (made != 0) {
            if (!unit.IsConstructing()) {
                player->SetFoodCap(std::max(0, player->GetFoodCap() - made));
            }
            unit.SetFoodMade(0);
        }
        const int used = unit.GetFoodUsed();
        if (used != 0) {
            player->SetFoodUsed(std::max(0, player->GetFoodUsed() - used));
            unit.SetFoodUsed(0);
        }
    }
    if (meleeVictoryRulesEnabled_) {
        EvaluateVictoryDefeat(sim);
    }
}

void War3GameModule::OnUnitMorphed(CSimulation& sim,
                                   CUnit& unit,
                                   const CUnitType* oldType,
                                   uint32_t oldTypeRawcode,
                                   const CUnitType* newType,
                                   uint32_t newTypeRawcode) {
    ++stats_.unitsMorphed;
    CPlayer* player = sim.GetPlayer(unit.GetPlayerIndex());
    if (!player) return;

    const War3ID oldId(oldTypeRawcode);
    const War3ID newId(newTypeRawcode);
    if (oldId != newId) {
        if (oldId.value != 0) {
            player->RemoveTechUnlocked(sim, oldId);
        }
        if (newId.value != 0) {
            player->AddTechUnlocked(sim, newId);
        }
    }

    if (oldType && newType) {
        const int oldFoodMade = unit.GetFoodMade();
        const int newFoodMade = newType->GetFoodMade();
        if (newFoodMade != oldFoodMade) {
            const int deltaMade = newFoodMade - oldFoodMade;
            if (deltaMade > 0) {
                player->AddFoodCap(deltaMade);
            } else {
                player->SetFoodCap(std::max(0, player->GetFoodCap() + deltaMade));
            }
            unit.SetFoodMade(newFoodMade);
        }

        const int oldFoodUsed = unit.GetFoodUsed();
        const int newFoodUsed = newType->GetFoodUsed();
        if (newFoodUsed != oldFoodUsed) {
            player->SetFoodUsed(std::max(0, player->GetFoodUsed() + (newFoodUsed - oldFoodUsed)));
            unit.SetFoodUsed(newFoodUsed);
        }
    }

    OnApplyPlayerUpgradesToUnit(sim, *player, unit);
}

void War3GameModule::OnUnitRemovedFromWorld(CSimulation& sim,
                                            CUnit& unit,
                                            bool wasActiveUnit) {
    ++stats_.unitsRemoved;
    if (!wasActiveUnit) return;
    if (CPlayer* player = sim.GetPlayer(unit.GetPlayerIndex())) {
        player->RemoveUnit(&unit);
        if (unit.IsHero()) {
            player->RemoveHero(&unit);
        }
        if (!unit.IsDead()) {
            player->RemoveTechUnlocked(sim, unit.GetTypeId());
        }
        if (unit.GetFoodUsed() > 0) {
            player->SetFoodUsed(std::max(0, player->GetFoodUsed() - unit.GetFoodUsed()));
        }
        if (unit.IsBuilding() && unit.GetFoodMade() > 0 && !unit.IsConstructing() && !unit.IsDead()) {
            player->AddFoodCap(-unit.GetFoodMade());
        }
    }
    if (meleeVictoryRulesEnabled_) {
        EvaluateVictoryDefeat(sim);
    }
}

// ============================================================================
// Phase 14: Techtree Equivalences, Upgrade Effect Engine & Upkeep Tax Rules
// ============================================================================

void War3GameModule::RegisterTechEquivalence(War3ID baseTechId, War3ID higherEquivalentId) {
    if (baseTechId.value == 0 || higherEquivalentId.value == 0 || baseTechId == higherEquivalentId) return;
    auto& list = customTechEquivalents_[baseTechId.value];
    if (std::find(list.begin(), list.end(), higherEquivalentId) == list.end()) {
        list.push_back(higherEquivalentId);
    }
}

std::vector<War3ID> War3GameModule::GetTechEquivalents(War3ID baseTechId) const {
    std::vector<War3ID> result;
    // Canonical WarCraft 3 town hall / tree / citadel / altar tier equivalences:
    // Owning a higher-tier hall satisfies techtree prerequisites for lower tiers.
    if (baseTechId == War3ID("htow")) {
        result.push_back(War3ID("hkee"));
        result.push_back(War3ID("hcas"));
    } else if (baseTechId == War3ID("hkee")) {
        result.push_back(War3ID("hcas"));
    } else if (baseTechId == War3ID("ogre")) {
        result.push_back(War3ID("ostr"));
        result.push_back(War3ID("ofor"));
        result.push_back(War3ID("ofrt"));
    } else if (baseTechId == War3ID("ostr")) {
        result.push_back(War3ID("ofor"));
        result.push_back(War3ID("ofrt"));
    } else if (baseTechId == War3ID("unpl")) {
        result.push_back(War3ID("unp1"));
        result.push_back(War3ID("unp2"));
    } else if (baseTechId == War3ID("unp1")) {
        result.push_back(War3ID("unp2"));
    } else if (baseTechId == War3ID("etol")) {
        result.push_back(War3ID("etoa"));
        result.push_back(War3ID("etoe"));
    } else if (baseTechId == War3ID("etoa")) {
        result.push_back(War3ID("etoe"));
    } else if (baseTechId == War3ID("hatw")) {
        result.push_back(War3ID("hwtw"));
        result.push_back(War3ID("hgtw"));
        result.push_back(War3ID("hctw"));
    }

    auto it = customTechEquivalents_.find(baseTechId.value);
    if (it != customTechEquivalents_.end()) {
        for (const War3ID& eq : it->second) {
            if (std::find(result.begin(), result.end(), eq) == result.end()) {
                result.push_back(eq);
            }
        }
    }
    return result;
}

int War3GameModule::GetEffectiveTechLevel(const CSimulation& sim,
                                          int playerIndex,
                                          uint32_t techRawcode) const {
    const CPlayer* player = sim.GetPlayer(playerIndex);
    if (!player) return 0;

    const War3ID baseId(techRawcode);
    int total = player->GetTechLevel(baseId);
    for (const War3ID& eqId : GetTechEquivalents(baseId)) {
        total += player->GetTechLevel(eqId);
    }
    return total;
}

bool War3GameModule::MeetsTechRequirements(const CSimulation& sim,
                                           int playerIndex,
                                           uint32_t targetRawcode) const {
    const CUnitData* unitData = sim.GetUnitData();
    if (!unitData) return true;

    const CUnitType* unitType = unitData->GetUnitType(War3ID(targetRawcode));
    if (!unitType) return true;

    const CPlayer* player = sim.GetPlayer(playerIndex);
    if (!player) return false;

    for (const auto& req : unitType->GetRequirements()) {
        const int requiredLevel = (req.level > 0) ? req.level : 1;
        if (GetEffectiveTechLevel(sim, playerIndex, req.id.value) < requiredLevel) {
            return false;
        }
    }
    return true;
}

std::vector<uint32_t> War3GameModule::GetMissingTechRequirements(const CSimulation& sim,
                                                                 int playerIndex,
                                                                 uint32_t targetRawcode) const {
    std::vector<uint32_t> missing;
    const CUnitData* unitData = sim.GetUnitData();
    if (!unitData) return missing;

    const CUnitType* unitType = unitData->GetUnitType(War3ID(targetRawcode));
    if (!unitType) return missing;

    const CPlayer* player = sim.GetPlayer(playerIndex);
    if (!player) return missing;

    for (const auto& req : unitType->GetRequirements()) {
        const int requiredLevel = (req.level > 0) ? req.level : 1;
        if (GetEffectiveTechLevel(sim, playerIndex, req.id.value) < requiredLevel) {
            missing.push_back(req.id.value);
        }
    }
    return missing;
}

bool War3GameModule::DoesUpgradeApplyToUnit(const CUnit& unit,
                                            const CUnitType& unitType,
                                            const CUpgradeType& upgrade) {
    const auto& used = unitType.GetUpgradesUsed();
    if (!used.empty()) {
        return std::find(used.begin(), used.end(), upgrade.id) != used.end();
    }
    // When unitType does not specify an explicit upgradesUsed list (e.g., synthetic
    // unit types in unit tests or universal upgrades), match by race or global scope:
    if (upgrade.race == 0) {
        return !unit.IsBuilding() || !upgrade.effects.empty();
    }
    const std::string& raceName = unitType.GetUnitRace().name;
    int unitRaceIdx = 0;
    if (raceName == "human" || raceName == "Human") unitRaceIdx = 1;
    else if (raceName == "orc" || raceName == "Orc") unitRaceIdx = 2;
    else if (raceName == "nightelf" || raceName == "night elf" || raceName == "NightElf") unitRaceIdx = 3;
    else if (raceName == "undead" || raceName == "Undead") unitRaceIdx = 4;
    if (unitRaceIdx == 0) return true;
    return unitRaceIdx == upgrade.race;
}

void War3GameModule::ApplySingleUpgradeToUnit(CSimulation& sim,
                                              CPlayer& player,
                                              CUnit& unit,
                                              const CUpgradeType& upgrade,
                                              int targetLevel) {
    if (targetLevel <= 0) return;
    const CUnitType* unitType = unit.GetUnitType();
    if (!unitType) return;
    if (!DoesUpgradeApplyToUnit(unit, *unitType, upgrade)) return;

    const int prevApplied = player.GetAppliedUnitUpgradeLevel(unit.GetHandleId(), upgrade.id);
    if (targetLevel <= prevApplied) return;

    const int deltaLevels = targetLevel - prevApplied;
    player.SetAppliedUnitUpgradeLevel(unit.GetHandleId(), upgrade.id, targetLevel);
    ++stats_.unitUpgradesApplied;

    if (!upgrade.effects.empty()) {
        for (const CUpgradeEffectSpec& eff : upgrade.effects) {
            if (eff.effectCode == "rarm") {
                const float perLevel = (eff.base != 0.0f || eff.mod != 0.0f)
                    ? eff.base
                    : static_cast<float>(std::max(1, unitType->GetDefenseUpgradeBonus()));
                const float deltaArmor = (eff.mod != 0.0f)
                    ? (eff.EvalCumulativeToLevel(targetLevel) - eff.EvalCumulativeToLevel(prevApplied))
                    : (perLevel * static_cast<float>(deltaLevels));
                unit.AddBonusArmor(static_cast<int>(std::round(deltaArmor)));
            } else if (eff.effectCode == "ratx" || eff.effectCode == "ratd" || eff.effectCode == "ratc") {
                int totalDmgBonus = 0;
                if (eff.base != 0.0f || eff.mod != 0.0f) {
                    totalDmgBonus = static_cast<int>(std::round(eff.EvalCumulativeToLevel(targetLevel)));
                } else {
                    int dmgUp = 2;
                    if (!unitType->GetAttacks().empty() && unitType->GetAttacks()[0].damageUpgradeAmount > 0) {
                        dmgUp = unitType->GetAttacks()[0].damageUpgradeAmount;
                    }
                    totalDmgBonus = dmgUp * targetLevel;
                }
                unit.Attrs().SetNonStackingDamageBonus(upgrade.id, totalDmgBonus);
            } else if (eff.effectCode == "rhpx") {
                const float deltaHp = eff.EvalCumulativeToLevel(targetLevel) - eff.EvalCumulativeToLevel(prevApplied);
                unit.AddBonusMaxLife(deltaHp);
                if (!unit.IsDead() && deltaHp > 0.0f) {
                    unit.SetLife(sim, std::min(unit.GetMaxLife(), unit.GetLife() + deltaHp));
                }
            } else if (eff.effectCode == "rhpo") {
                const float baseHp = static_cast<float>(unit.Attrs().GetBaseMaxLife());
                const float deltaHp = baseHp * (eff.EvalCumulativeToLevel(targetLevel) - eff.EvalCumulativeToLevel(prevApplied));
                unit.AddBonusMaxLife(deltaHp);
                if (!unit.IsDead() && deltaHp > 0.0f) {
                    unit.SetLife(sim, std::min(unit.GetMaxLife(), unit.GetLife() + deltaHp));
                }
            } else if (eff.effectCode == "rmnx" || eff.effectCode == "rmno") {
                const float baseMana = static_cast<float>(unit.Attrs().GetBaseMaxMana());
                const float rawDelta = (eff.effectCode == "rmno")
                    ? (baseMana * (eff.EvalCumulativeToLevel(targetLevel) - eff.EvalCumulativeToLevel(prevApplied)))
                    : (eff.EvalCumulativeToLevel(targetLevel) - eff.EvalCumulativeToLevel(prevApplied));
                const int deltaMana = static_cast<int>(std::round(rawDelta));
                unit.AddBonusMaxMana(deltaMana);
                if (!unit.IsDead() && deltaMana > 0) {
                    unit.SetMana(std::min(static_cast<float>(unit.GetMaxMana()),
                                          unit.GetMana() + static_cast<float>(deltaMana)));
                }
            } else if (eff.effectCode == "rmvx") {
                const float rawSpeedBonus = eff.EvalCumulativeToLevel(targetLevel);
                const float pct = (rawSpeedBonus > 1.5f)
                    ? (rawSpeedBonus / std::max(1.0f, static_cast<float>(unit.Attrs().GetBaseSpeed())))
                    : rawSpeedBonus;
                unit.Attrs().AddSpeedModifier(upgrade.id, pct);
            } else if (eff.effectCode == "rats") {
                unit.Attrs().SetNonStackingAttackSpeed(upgrade.id, eff.EvalCumulativeToLevel(targetLevel));
            } else if (eff.effectCode == "rlev") {
                if (eff.code.size() == 4) {
                    if (CAbility* ab = unit.GetAbility(War3ID::FromString(eff.code))) {
                        const int addLvl = std::max(deltaLevels,
                            static_cast<int>(std::round(eff.EvalCumulativeToLevel(targetLevel))));
                        ab->SetLevel(1 + addLvl);
                        ab->OnLevelChanged(sim, &unit);
                    }
                }
            }
        }
    } else if (upgrade.upgradeClass == "armor") {
        const float perLevel = static_cast<float>(std::max(1, unitType->GetDefenseUpgradeBonus()));
        unit.AddBonusArmor(static_cast<int>(std::round(perLevel * static_cast<float>(deltaLevels))));
    } else if (upgrade.upgradeClass == "melee" || upgrade.upgradeClass == "ranged" ||
               upgrade.upgradeClass == "artillery") {
        int dmgUp = 2;
        if (!unitType->GetAttacks().empty() && unitType->GetAttacks()[0].damageUpgradeAmount > 0) {
            dmgUp = unitType->GetAttacks()[0].damageUpgradeAmount;
        }
        unit.Attrs().SetNonStackingDamageBonus(upgrade.id, dmgUp * targetLevel);
    }
}

void War3GameModule::RefreshUnitAbilityRequirements(CSimulation& sim, CUnit& unit) {
    for (CAbility* ab : unit.GetAbilities()) {
        if (!ab) continue;
        const bool wasDisabled = ab->IsDisabled();
        const bool met = ab->IsRequirementsMet(sim, &unit);
        ab->SetDisabled(!met, CAbilityDisableType::REQUIREMENTS);
        if (wasDisabled && !ab->IsDisabled()) {
            ab->OnAdd(sim, &unit);
        }
    }
}

void War3GameModule::OnPlayerTechLevelChanged(CSimulation& sim,
                                              CPlayer& player,
                                              uint32_t techRawcode,
                                              int /*oldLevel*/,
                                              int newLevel) {
    ++stats_.techEventsProcessed;
    const War3ID techId(techRawcode);
    const CUpgradeData* upgradeData = sim.GetUpgradeData();
    const CUpgradeType* upgrade = upgradeData ? upgradeData->GetUpgrade(techId) : nullptr;

    for (CUnit* unit : player.GetUnits()) {
        if (!unit || unit->IsDead()) continue;
        if (upgrade && newLevel > 0) {
            ApplySingleUpgradeToUnit(sim, player, *unit, *upgrade, newLevel);
        }
        RefreshUnitAbilityRequirements(sim, *unit);
    }
}

void War3GameModule::OnApplyPlayerUpgradesToUnit(CSimulation& sim,
                                                 CPlayer& player,
                                                 CUnit& unit) {
    const CUpgradeData* upgradeData = sim.GetUpgradeData();
    if (upgradeData) {
        for (const auto& [rawCode, level] : player.GetTechtreeUnlockedMap()) {
            if (level <= 0) continue;
            if (const CUpgradeType* upgrade = upgradeData->GetUpgrade(War3ID(rawCode))) {
                ApplySingleUpgradeToUnit(sim, player, unit, *upgrade, level);
            }
        }
    }
    RefreshUnitAbilityRequirements(sim, unit);
}

int War3GameModule::ComputeUpkeepLevel(const CPlayer& player) const {
    const int foodUsed = player.GetFoodUsed();
    if (foodUsed > 80) return 2; // High Upkeep (60% tax)
    if (foodUsed > 50) return 1; // Low Upkeep (30% tax)
    return 0;                    // No Upkeep (0% tax)
}

int War3GameModule::ApplyUpkeepGoldTax(const CPlayer& player, int amount) const {
    if (amount <= 0) return amount;
    const int explicitRate = player.GetGoldUpkeepRate();
    const int upkeepLvl = ComputeUpkeepLevel(player);
    const int taxPct = explicitRate > 0 ? explicitRate
                     : (upkeepLvl == 2 ? 60 : upkeepLvl == 1 ? 30 : 0);
    if (taxPct <= 0) return amount;
    if (taxPct >= 100) return 0;
    return amount - (amount * taxPct) / 100;
}

// ============================================================================
// Phase 14: Melee & Allied Victory/Defeat & Crippled Rules Engine
// ============================================================================

bool War3GameModule::ArePlayersAlliedForVictory(const CPlayer& a, const CPlayer& b) {
    if (a.GetId() == b.GetId()) return true;
    if (a.GetTeam() >= 0 && a.GetTeam() == b.GetTeam() &&
        a.HasAlliedVictory() && b.HasAlliedVictory()) {
        return true;
    }
    return a.HasAlliedVictory() && b.HasAlliedVictory() &&
           a.HasAlliance(b, CAllianceType::ALLIED) &&
           b.HasAlliance(a, CAllianceType::ALLIED);
}

bool War3GameModule::IsPlayerCrippled(const CSimulation& sim, int playerIndex) const {
    const CPlayer* player = sim.GetPlayer(playerIndex);
    if (!player || !player->IsPlaying()) return false;
    const CMapControl ctrl = player->GetController();
    if (ctrl != CMapControl::USER && ctrl != CMapControl::COMPUTER) return false;

    int livingBuildings = 0;
    int livingTownHalls = 0;
    int livingWorkers = 0;

    for (const CUnit* u : sim.GetUnits()) {
        if (!u || u->IsDead() || u->GetPlayerIndex() != playerIndex) continue;
        if (u->IsBuilding()) {
            ++livingBuildings;
            const CUnitType* ut = u->GetUnitType();
            const bool isHall = IsTownHallReturnResourceType(u->GetTypeId()) ||
                (ut && ut->GetClassifications().count(CUnitClassification::TOWNHALL) > 0);
            if (isHall) {
                ++livingTownHalls;
            }
        } else {
            if (IsStandardRaceWorkerType(u->GetTypeId()) ||
                u->GetAbility(War3ID("Ahar")) != nullptr ||
                u->GetAbility(War3ID("AHbu")) != nullptr ||
                u->GetAbility(War3ID("AObu")) != nullptr ||
                u->GetAbility(War3ID("AUbu")) != nullptr ||
                u->GetAbility(War3ID("AEbu")) != nullptr) {
                ++livingWorkers;
            }
        }
    }

    if (livingBuildings <= 0 || livingTownHalls > 0) {
        return false;
    }

    // Standard War3 melee crippled condition: no town hall standing, and either
    // no worker alive to build one OR insufficient resources for a base hall (385g / 180l).
    constexpr int kMinTownHallGold = 385;
    constexpr int kMinTownHallLumber = 180;
    if (livingWorkers == 0) return true;
    return player->GetGold() < kMinTownHallGold || player->GetLumber() < kMinTownHallLumber;
}

void War3GameModule::OnPlayerRemoved(CSimulation& sim, int playerIndex, int gameResultCode) {
    if (CPlayer* p = sim.GetPlayer(playerIndex)) {
        p->SetSlotActive(false);
        p->SetGameResultCode(gameResultCode);
    }
    if (meleeVictoryRulesEnabled_) {
        EvaluateVictoryDefeat(sim);
    }
}

void War3GameModule::EvaluateVictoryDefeat(CSimulation& sim) {
    ++stats_.victoryEvaluations;
    if (matchEnded_) return;

    std::vector<CPlayer*> competitors;
    for (int i = 0; i < sim.GetPlayerCount(); ++i) {
        CPlayer* p = sim.GetPlayer(i);
        if (!p || p->IsObserver()) continue;
        const CMapControl ctrl = p->GetController();
        if (ctrl == CMapControl::USER || ctrl == CMapControl::COMPUTER) {
            competitors.push_back(p);
        }
    }

    if (competitors.empty()) return;

    // 1. Evaluate individual competitor elimination (Defeat) and Crippled status
    for (CPlayer* p : competitors) {
        if (p->GetGameResult() != CPlayerGameResult::PLAYING) continue;

        int livingStructures = 0;
        int livingUnits = 0;
        for (const CUnit* u : sim.GetUnits()) {
            if (!u || u->IsDead() || u->GetPlayerIndex() != p->GetId()) continue;
            ++livingUnits;
            if (u->IsBuilding()) {
                ++livingStructures;
            }
        }
        // Also count pending newly created units not yet committed to units_
        for (const CUnit* u : p->GetUnits()) {
            if (!u || u->IsDead()) continue;
            if (std::find(sim.GetUnits().begin(), sim.GetUnits().end(), u) != sim.GetUnits().end()) continue;
            ++livingUnits;
            if (u->IsBuilding()) {
                ++livingStructures;
            }
        }

        const bool eliminated = !p->IsSlotActive() ||
            (livingStructures == 0 && (!requireZeroUnitsForDefeat_ || livingUnits == 0));

        if (eliminated) {
            if (!p->IsNoDefeatCheat()) {
                p->SetGameResult(CPlayerGameResult::DEFEAT);
                p->SetCrippled(false);
                sim.Events().onPlayerDefeated.Emit(p->GetId());
            }
        } else {
            const bool nowCrippled = IsPlayerCrippled(sim, p->GetId());
            if (nowCrippled != p->IsCrippled()) {
                p->SetCrippled(nowCrippled);
                sim.Events().onPlayerCrippled.Emit(p->GetId(), nowCrippled);
            }
        }
    }

    // 2. Evaluate surviving competitors for solo or allied Victory
    std::vector<CPlayer*> survivors;
    int defeatedCount = 0;
    for (CPlayer* p : competitors) {
        if (p->GetGameResult() == CPlayerGameResult::PLAYING && p->IsSlotActive()) {
            survivors.push_back(p);
        } else if (p->GetGameResult() == CPlayerGameResult::DEFEAT || !p->IsSlotActive()) {
            ++defeatedCount;
        }
    }

    if (survivors.empty()) {
        if (defeatedCount > 0) {
            matchEnded_ = true;
            winningPlayerIndex_ = -1;
            sim.Events().onGameEnd.Emit(false, -1);
        }
        return;
    }

    // Require at least 2 competitors (or at least 1 defeated opponent) to declare melee victory
    if (competitors.size() < 2 && defeatedCount == 0) {
        return;
    }

    bool allSurvivorsAllied = true;
    for (size_t i = 0; i < survivors.size() && allSurvivorsAllied; ++i) {
        for (size_t j = i + 1; j < survivors.size(); ++j) {
            if (!ArePlayersAlliedForVictory(*survivors[i], *survivors[j])) {
                allSurvivorsAllied = false;
                break;
            }
        }
    }

    if (allSurvivorsAllied) {
        bool anyBlockedByNoVictoryCheat = false;
        for (CPlayer* s : survivors) {
            if (s->IsNoVictoryCheat()) {
                anyBlockedByNoVictoryCheat = true;
                break;
            }
        }
        if (!anyBlockedByNoVictoryCheat) {
            winningPlayerIndex_ = survivors.front()->GetId();
            matchEnded_ = true;
            for (CPlayer* s : survivors) {
                s->SetGameResult(CPlayerGameResult::VICTORY);
                sim.Events().onPlayerVictorious.Emit(s->GetId());
            }
            sim.Events().onGameEnd.Emit(true, winningPlayerIndex_);
        }
    }
}

// ============================================================================
// Phase 15: Combat Armor/Attack Type Multiplier Matrix & Damage Formula Engine
// ============================================================================

const AttackDefenseTable& War3GameModule::GetCanonicalWar3AttackDefenseTable() {
    // Canonical WarCraft III: The Frozen Throne Attack-vs-Defense multiplier matrix
    // Columns indexed by CCombatSystem::DefenseTypeToIndex:
    // [0]=NONE, [1]=LIGHT, [2]=MEDIUM, [3]=HEAVY, [4]=FORTIFIED, [5]=HERO, [6]=DIVINE, [7]=UNARMORED
    static const AttackDefenseTable kCanonicalWar3Table = {
        // Normal: 100 / 100 / 150 / 100 / 70 / 100 / 5 / 100
        { 1.0f, 1.0f, 1.5f, 1.0f, 0.7f, 1.0f, 0.05f, 1.0f },
        // Pierce: 100 / 200 / 75 / 100 / 35 / 50 / 5 / 150
        { 1.0f, 2.0f, 0.75f, 1.0f, 0.35f, 0.5f, 0.05f, 1.5f },
        // Siege:  100 / 100 / 50 / 100 / 150 / 50 / 5 / 150
        { 1.0f, 1.0f, 0.5f, 1.0f, 1.5f, 0.5f, 0.05f, 1.5f },
        // Magic:  100 / 125 / 75 / 200 / 35 / 50 / 5 / 100
        { 1.0f, 1.25f, 0.75f, 2.0f, 0.35f, 0.5f, 0.05f, 1.0f },
        // Chaos:  100% across all armor types
        { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f },
        // Hero:   100% except Fortified 50%, Divine 5%
        { 1.0f, 1.0f, 1.0f, 1.0f, 0.5f, 1.0f, 0.05f, 1.0f },
        // Spells: 100% except Hero 70%, Divine 5%
        { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.7f, 0.05f, 1.0f }
    };
    return kCanonicalWar3Table;
}

void War3GameModule::OnConfigureCombatSystem(CSimulation& /*sim*/, CCombatSystem& combat) {
    combat.SetDefenseArmorFactor(defenseArmorFactor_);
    combat.ResetTableToModuleDefault(combatTable_);
}

float War3GameModule::GetAttackDefenseMultiplier(CAttackType atk, CDefenseType def) const {
    return GetAttackDefenseMultiplier(static_cast<int>(atk), static_cast<int>(def));
}

float War3GameModule::GetAttackDefenseMultiplier(int attackTypeOrd, int defenseTypeOrd) const {
    const auto atk = static_cast<CAttackType>(attackTypeOrd);
    const auto def = static_cast<CDefenseType>(defenseTypeOrd);
    int di = CCombatSystem::DefenseTypeToIndex(def);
    if (di < 0 || di > 7) di = 0;

    switch (atk) {
        case CAttackType::NORMAL: return combatTable_.normal[di];
        case CAttackType::PIERCE: return combatTable_.pierce[di];
        case CAttackType::SIEGE:  return combatTable_.siege[di];
        case CAttackType::MAGIC:  return combatTable_.magic[di];
        case CAttackType::SPELLS: return combatTable_.spells[di];
        case CAttackType::CHAOS:  return combatTable_.chaos[di];
        case CAttackType::HERO:   return combatTable_.hero[di];
        default: return 1.0f;
    }
}

void War3GameModule::SetAttackDefenseMultiplier(CAttackType atk, CDefenseType def, float multiplier) {
    int di = CCombatSystem::DefenseTypeToIndex(def);
    if (di < 0 || di > 7) return;
    switch (atk) {
        case CAttackType::NORMAL: combatTable_.normal[di] = multiplier; break;
        case CAttackType::PIERCE: combatTable_.pierce[di] = multiplier; break;
        case CAttackType::SIEGE:  combatTable_.siege[di]  = multiplier; break;
        case CAttackType::MAGIC:  combatTable_.magic[di]  = multiplier; break;
        case CAttackType::SPELLS: combatTable_.spells[di] = multiplier; break;
        case CAttackType::CHAOS:  combatTable_.chaos[di]  = multiplier; break;
        case CAttackType::HERO:   combatTable_.hero[di]   = multiplier; break;
        default: break;
    }
    if (boundSim_ && boundSim_->GetCombatSystem()) {
        boundSim_->GetCombatSystem()->ResetTableToModuleDefault(combatTable_);
    }
}

void War3GameModule::SetCombatTable(const AttackDefenseTable& table) {
    combatTable_ = table;
    if (boundSim_ && boundSim_->GetCombatSystem()) {
        boundSim_->GetCombatSystem()->ResetTableToModuleDefault(combatTable_);
    }
}

void War3GameModule::ResetCombatRulesToCanonicalDefaults() {
    combatTable_ = GetCanonicalWar3AttackDefenseTable();
    defenseArmorFactor_ = 0.06f;
    if (boundSim_ && boundSim_->GetCombatSystem()) {
        OnConfigureCombatSystem(*boundSim_, *boundSim_->GetCombatSystem());
    }
}

float War3GameModule::ComputeArmorDamageReduction(int defense,
                                                  int attackerLevel,
                                                  float defenseArmorFactor) const {
    ++stats_.combatDamageEvaluations;
    const float k = (defenseArmorFactor > 0.0f) ? defenseArmorFactor : defenseArmorFactor_;
    return CDefenseTypeInfo::CalcDamageReduction(defense, attackerLevel, k);
}

bool War3GameModule::ShouldResistMagicDamage(int attackTypeOrd,
                                             int damageTypeOrd,
                                             bool targetIsMagicImmune,
                                             bool magicImmuneResistsDamage,
                                             bool magicImmuneResistsUltimates) const {
    if (!targetIsMagicImmune) return false;
    const auto atk = static_cast<CAttackType>(attackTypeOrd);
    const auto dmg = static_cast<CDamageType>(damageTypeOrd);
    if (atk == CAttackType::MAGIC && magicImmuneResistsDamage) {
        return true;
    }
    if (atk == CAttackType::SPELLS && dmg != CDamageType::NORMAL) {
        if (dmg != CDamageType::UNIVERSAL && magicImmuneResistsUltimates) {
            return true;
        }
    }
    return false;
}

bool War3GameModule::EvaluateEtherealDamageFactor(int attackTypeOrd,
                                                  int damageTypeOrd,
                                                  bool targetIsEthereal,
                                                  bool allied,
                                                  bool etherealBonusAlly,
                                                  float etherealBonusMagic,
                                                  float etherealBonusSpells,
                                                  float& outMultiplier) const {
    outMultiplier = 1.0f;
    if (!targetIsEthereal) return true;
    const auto atk = static_cast<CAttackType>(attackTypeOrd);
    const auto dmg = static_cast<CDamageType>(damageTypeOrd);
    const bool grantBonus = !allied || etherealBonusAlly;
    if (grantBonus && atk == CAttackType::MAGIC) {
        outMultiplier = etherealBonusMagic;
    } else if (grantBonus && atk == CAttackType::SPELLS) {
        outMultiplier = etherealBonusSpells;
    }
    if (dmg == CDamageType::NORMAL && atk != CAttackType::MAGIC) {
        return false; // Ethereal units are immune to normal physical attacks
    }
    return true;
}

// ============================================================================
// Phase 15: Tactical AI Controller Lifecycle, Build-Order Discovery & Counter-Scoring
// ============================================================================

bool War3GameModule::CreatePlayerAI(CSimulation& sim, int playerIndex, int difficulty) {
    if (playerIndex < 0 || playerIndex >= 24) return false;
    auto controller = std::make_shared<MeleeAIController>(&sim, playerIndex);
    controller->SetDifficulty(difficulty);
    aiControllers_[playerIndex] = std::move(controller);
    ++stats_.aiControllersCreated;
    return true;
}

void War3GameModule::AttachPlayerAI(int playerIndex, std::shared_ptr<MeleeAIController> controller) {
    if (playerIndex < 0 || playerIndex >= 24 || !controller) return;
    aiControllers_[playerIndex] = std::move(controller);
    ++stats_.aiControllersCreated;
}

MeleeAIController* War3GameModule::GetPlayerAI(int playerIndex) const {
    auto it = aiControllers_.find(playerIndex);
    return (it != aiControllers_.end()) ? it->second.get() : nullptr;
}

bool War3GameModule::RemovePlayerAI(int playerIndex) {
    return aiControllers_.erase(playerIndex) > 0;
}

void War3GameModule::SetPlayerAIEnabled(int playerIndex, bool enabled) {
    if (MeleeAIController* ai = GetPlayerAI(playerIndex)) {
        ai->SetEnabled(enabled);
    }
}

void War3GameModule::CommandPlayerAI(int playerIndex, int command, int data) {
    if (MeleeAIController* ai = GetPlayerAI(playerIndex)) {
        ai->Command(command, data);
    }
}

void War3GameModule::TickPlayerAIs(CSimulation& /*sim*/, float /*dt*/) {
    for (auto& [playerId, controller] : aiControllers_) {
        (void)playerId;
        if (controller && controller->IsEnabled()) {
            controller->Update();
            ++stats_.aiControllerTicks;
        }
    }
}

float War3GameModule::EvaluateTacticalTargetScore(const CSimulation& /*sim*/,
                                                  const CUnit& attacker,
                                                  const CUnit& candidateTarget,
                                                  float baseScore) const {
    ++stats_.tacticalTargetEvaluations;
    const CUnitType* atkUnitType = attacker.GetUnitType();
    if (!atkUnitType || atkUnitType->GetAttacks().empty()) {
        return baseScore;
    }

    const CAttackType atkType = atkUnitType->GetAttacks()[0].attackType;
    const CDefenseType defType = candidateTarget.GetDefenseType();
    const float counterMult = GetAttackDefenseMultiplier(atkType, defType);

    // Reward hard armor-vs-attack counters (e.g., Pierce 2.0x vs Light -> +90 pts,
    // Magic 2.0x vs Heavy -> +90 pts, Normal 1.5x vs Medium -> +45 pts) and penalize
    // heavily resisted targets (e.g., Pierce 0.35x vs Fortified -> -58.5 pts).
    const float counterBonus = (counterMult - 1.0f) * 90.0f;
    return std::max(0.01f, baseScore + counterBonus);
}

bool War3GameModule::DiscoverRaceBuildPlan(const CSimulation& sim,
                                           int playerIndex,
                                           AIRacePlan& outPlan) {
    outPlan = AIRacePlan{};
    CUnitData* unitData = sim.GetUnitData();
    if (!unitData) return false;

    std::string race;
    const CUnitType* townHallType = nullptr;
    for (CUnit* unit : sim.GetUnits()) {
        if (!unit || unit->IsDead() || unit->GetPlayerIndex() != playerIndex) continue;
        const CUnitType* type = unit->GetUnitType();
        if (!type || !type->IsBuilding() || type->GetUnitsTrained().empty()) continue;
        townHallType = type;
        race = type->GetUnitRace().name;
        outPlan.townHall = unit->GetTypeId();
        break;
    }
    if (!townHallType || race.empty()) return false;

    War3ID worker;
    int bestWorkerCost = INT32_MAX;
    for (War3ID id : townHallType->GetUnitsTrained()) {
        const CUnitType* type = unitData->GetUnitType(id);
        if (!type) continue;
        if (MeleeAIController::IsWorkerType(type) || IsStandardRaceWorkerType(id)) {
            worker = id;
            break;
        }
        const int cost = type->GetGoldCost() + type->GetLumberCost();
        if (cost < bestWorkerCost) {
            bestWorkerCost = cost;
            worker = id;
        }
    }
    if (worker.value == 0) return false;
    outPlan.worker = worker;

    int bestFarmCost = INT32_MAX;
    for (const auto& [id, type] : unitData->GetAllTypes()) {
        if (!type || !type->IsBuilding()) continue;
        if (type->GetUnitRace().name != race) continue;
        if (id != outPlan.townHall && type->GetFoodMade() > 0) {
            const int cost = type->GetGoldCost() + type->GetLumberCost();
            if (cost < bestFarmCost) {
                bestFarmCost = cost;
                outPlan.farm = id;
            }
        }
        if (id == outPlan.townHall || outPlan.barracks.value != 0) continue;
        for (War3ID trainedId : type->GetUnitsTrained()) {
            const CUnitType* trained = unitData->GetUnitType(trainedId);
            if (MeleeAIController::IsCombatType(trained)) {
                outPlan.barracks = id;
                break;
            }
        }
    }

    if (outPlan.barracks.value != 0) {
        const CUnitType* barracksType = unitData->GetUnitType(outPlan.barracks);
        int bestArmyCost = INT32_MAX;
        if (barracksType) {
            for (War3ID id : barracksType->GetUnitsTrained()) {
                const CUnitType* type = unitData->GetUnitType(id);
                if (!MeleeAIController::IsCombatType(type)) continue;
                const int cost = type->GetGoldCost() + type->GetLumberCost();
                if (cost < bestArmyCost) {
                    bestArmyCost = cost;
                    outPlan.armyUnit = id;
                }
            }
        }
    }

    for (const auto& [id, type] : unitData->GetAllTypes()) {
        if (!type || !type->IsBuilding()) continue;
        if (type->GetUnitRace().name != race) continue;
        if (id == outPlan.townHall) continue;
        int bestHeroCost = INT32_MAX;
        War3ID bestHero;
        for (War3ID trainedId : type->GetUnitsTrained()) {
            const CUnitType* trained = unitData->GetUnitType(trainedId);
            if (!trained || !trained->IsHero()) continue;
            const int cost = trained->GetGoldCost() + trained->GetLumberCost();
            if (cost < bestHeroCost) {
                bestHeroCost = cost;
                bestHero = trainedId;
            }
        }
        if (bestHero.value != 0) {
            outPlan.altar = id;
            outPlan.hero = bestHero;
            break;
        }
    }

    int bestTowerCost = INT32_MAX;
    for (const auto& [id, type] : unitData->GetAllTypes()) {
        if (!MeleeAIController::IsTowerType(type.get())) continue;
        if (type->GetUnitRace().name != race) continue;
        const int cost = type->GetGoldCost() + type->GetLumberCost();
        if (cost < bestTowerCost) {
            bestTowerCost = cost;
            outPlan.tower = id;
        }
    }

    outPlan.valid = true;
    return true;
}

} // namespace war3
