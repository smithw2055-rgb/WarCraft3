#include "game/war3/war3_game_module.h"

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

#include <cmath>
#include "data/abilitydata.h"
#include "data/destructabledata.h"
#include "ecs/buffs.h"
#include "ecs/hero_carry.h"
#include "ecs/combat/projectile.h"
#include "ecs/behaviors/behavior_move_into_range.h"
#include "ecs/item_bonuses.h"
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
#include "ecs/orders/order.h"
#include "ecs/ability_factory.h"
#include "ecs/csimulation.h"
#include "ecs/destructable.h"
#include "ecs/item.h"
#include "ecs/player.h"
#include "ecs/unit.h"
#include "resource/resource_server.h"

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
    REG_T("Arev", CAbilityReviveHero);

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

    // Batch 22 retail aliases (formerly special-cased in AbilityFactory)
    REG_ALIAS("Afod", CAbilityFingerOfDeath);
    REG_ALIAS("Acht", CAbilityHowlOfTerror);
    REG_ALIAS("ANin", CAbilityInferno);
    REG_ALIAS("Aams", CAbilityAntiMagicShell);

    #undef REG_T
    #undef REG_ALIAS

    AbilityBuilderRegistry::Instance().RegisterStandardCanonicalDescriptors();
}

size_t War3GameModule::UnregisterWar3AbilityCatalog() {
    return AbilityFactory::UnregisterModuleCreators(kModuleId);
}

void War3GameModule::RegisterCustomSpellDescriptor(const ABSpellDescriptor& desc) {
    AbilityFactory::RegisterModuleSpellDescriptor(kModuleId, desc);
    ++stats_.customSpellsRegistered;
}

size_t War3GameModule::GetRegisteredAbilityCount() const {
    return AbilityFactory::GetModuleRegisteredCount(kModuleId);
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
    customAutocastKinds_.clear();
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

    // 2. Heroes receive the Hero ability (attributes, XP, skill points, and learnable heroAbilList)
    if (unitType.IsHero()) {
        auto heroAbility = std::make_unique<CAbilityHero>(sim.AllocateHandleId());
        for (const auto& skillCode : unitType.GetHeroAbilityList()) {
            heroAbility->AddLearnableSkill(skillCode);
        }
        unit.AddAbility(sim, std::move(heroAbility));
    }

    // 3. Buildings receive Queue, Rally, Upgrade, Shop SellItems, GoldMine, and ReturnResources
    if (unitType.IsBuilding()) {
        unit.AddAbility(sim, std::make_unique<CAbilityQueue>(sim.AllocateHandleId()));
        unit.AddAbility(sim, std::make_unique<CAbilityRally>(sim.AllocateHandleId()));
        unit.AddAbility(sim, std::make_unique<CAbilityUpgrade>(sim.AllocateHandleId()));

        if (CAbility* queueAbility = unit.GetAbility(War3ID("Aque"))) {
            for (const auto& trainId : unitType.GetUnitsTrained()) {
                queueAbility->AddTrainedUnit(trainId);
            }
            for (const auto& researchId : unitType.GetResearchesAvailable()) {
                queueAbility->AddResearch(researchId);
            }
        }
        if (CAbility* upgradeAbility = unit.GetAbility(War3ID("Aupg"))) {
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


// ============================================================================
// Phase 16: Unit Combat Reactions, Passive Procs, Kill Bounty & Hero XP,
//           Idle Spell Autocast, Item Bonuses & Campaign Hero Carry-Over
// ============================================================================

void War3GameModule::OnUnitAbilityAdded(CSimulation& sim,
                                        CUnit& unit,
                                        CAbility& ability) {
    CAbilityData* data = sim.GetAbilityData();
    if (!data) return;

    if (ability.IsSpellAbility()) {
        if (const CAbilityDefFull* def = data->GetAbility(ability.GetAlias())) {
            ability.ApplyData(*def);
        }
    }

    if (const CAbilityDefFull* def = data->GetAbility(ability.GetAlias())) {
        if (ability.GetCategory() == CAbilityCategory::CARGO_HOLD && ability.GetCargoCapacity() > 0) {
            const int cap = def->DataIntAt(1, 0, ability.GetCargoCapacity());
            ability.SetCargoCapacity(cap);
        }
        if (ability.GetCode() == War3ID("Aloa")) {
            const float range = def->RangeAt(1);
            if (range > 0.0f) ability.SetLoadRange(range);
            if (CAbility* hold = unit.GetAbility(War3ID("Acar"))) {
                if (hold->GetCargoCapacity() > 0) {
                    ability.SetCargoCapacity(hold->GetCargoCapacity());
                }
            }
        }
    }
}

float War3GameModule::GetNegativeBuffDurationMultiplier(const CUnit& target) const {
    // Resistant Skin (Arsk / Brsk): negative effects wear off in 25% of normal time.
    return target.HasBuff(War3ID("Brsk")) ? 0.25f : 1.0f;
}

bool War3GameModule::OnCheckSpellEffectReaction(CSimulation& sim,
                                                CUnit& target,
                                                CUnit* caster) {
    if (!caster || caster == &target) return true;
    CPlayer* self = sim.GetPlayer(target.GetPlayerIndex());
    CPlayer* foe = sim.GetPlayer(caster->GetPlayerIndex());
    if (self && foe && self->HasAlliance(*foe, CAllianceType::PASSIVE)) {
        return true;
    }
    if (!target.HasBuff(War3ID("BNss"))) return true;
    target.RemoveBuff(War3ID("BNss"), sim);
    return false;
}

bool War3GameModule::OnEvaluatePreMitigationDamage(CSimulation& sim,
                                                   CUnit* attacker,
                                                   CUnit& target,
                                                   bool isAttack,
                                                   bool /*isRanged*/,
                                                   int attackTypeOrd,
                                                   int /*damageTypeOrd*/,
                                                   bool invulnerableTarget,
                                                   float& inOutDamage) {
    ++stats_.preMitigationEvaluations;
    const auto attackType = static_cast<CAttackType>(attackTypeOrd);

    // 1. Drunken Haze (BNdh) & Curse (Bcrs): attacker swing may miss outright via CBuff::GetMissChance()
    if (isAttack && attacker && attacker != &target) {
        if (const CBuff* haze = attacker->GetBuff(War3ID("BNdh"))) {
            if (sim.SimRand01() < haze->GetMissChance()) return false;
        }
        if (const CBuff* curse = attacker->GetBuff(War3ID("Bcrs"))) {
            if (sim.SimRand01() < curse->GetMissChance()) return false;
        }
    }

    // 2. Highland cliff accuracy check (25% miss firing uphill)
    if (sim.CheckHighlandMiss(attacker, &target, isAttack)) {
        sim.FireCombatText(attacker ? attacker->GetHandleId() : -1,
                           target.GetHandleId(),
                           CSimulation::CombatTextKind::MISS,
                           0.0f);
        return false;
    }

    // 3. Mana Shield (BNms): incoming damage drains mana first (suppressed while invulnerable)
    if (!invulnerableTarget && inOutDamage > 0.0f && target.GetMana() > 0.0f &&
        target.GetBuff(War3ID("BNms"))) {
        float absorbed = inOutDamage;
        if (absorbed > target.GetMana()) absorbed = target.GetMana();
        target.SetMana(target.GetMana() - absorbed);
        inOutDamage -= absorbed;
        if (inOutDamage <= 0.0f) return false;
    }

    // 4. Howl of Terror (BNht) & Item reaction AIdd attacker damage-dealt multiplier via CBuff virtual contract
    if (isAttack && attacker && attacker != &target) {
        if (const CBuff* howl = attacker->GetBuff(War3ID("BNht"))) {
            inOutDamage *= howl->GetDamageDealtFactor();
        }
        float dealt = 1.0f;
        for (const auto& buff : attacker->GetBuffs()) {
            if (buff) dealt += buff->GetAttackDealtAdd(), inOutDamage *= (buff->GetId() != War3ID("BNht") ? buff->GetDamageDealtFactor() : 1.0f);
        }
        if (dealt < 0.0f) dealt = 0.0f;
        inOutDamage *= dealt;
    }

    // 5. Avatar (BAvt): spell immunity while Mountain King's Avatar is active
    if (!invulnerableTarget && attackType == CAttackType::SPELLS &&
        target.GetBuff(War3ID("BAvt"))) {
        return false;
    }

    return true;
}

void War3GameModule::OnModifyPostMitigationDamage(CSimulation& /*sim*/,
                                                  CUnit* /*attacker*/,
                                                  CUnit& target,
                                                  bool isAttack,
                                                  bool /*isRanged*/,
                                                  int attackTypeOrd,
                                                  int /*damageTypeOrd*/,
                                                  float& inOutActualDamage) {
    ++stats_.postMitigationEvaluations;
    if (inOutActualDamage <= 0.0f) return;

    // 1. Defend (0.5x weapon attack reduction) & Berserk (damage-taken amplification) via CBuff virtual contract
    if (isAttack) {
        for (const auto& buff : target.GetBuffs()) {
            if (buff && buff->GetId() == War3ID("Bdef")) {
                inOutActualDamage *= buff->GetWeaponDamageTakenMultiplier();
                break;
            }
        }
        for (const auto& buff : target.GetBuffs()) {
            if (buff && buff->GetId() != War3ID("Bdef") && buff->GetWeaponDamageTakenMultiplier() != 1.0f) {
                inOutActualDamage *= buff->GetWeaponDamageTakenMultiplier();
                break;
            }
        }
    }

    // 2. Runed Bracers / item per-attack-type damage-taken modifiers via CBuff virtual contract
    for (const auto& buff : target.GetBuffs()) {
        if (!buff) continue;
        inOutActualDamage *= buff->GetAttackTypeDamageTakenMultiplier(attackTypeOrd);
    }
}

void War3GameModule::OnPostDamageLanded(CSimulation& sim,
                                        CUnit* attacker,
                                        CUnit& target,
                                        bool isAttack,
                                        bool isRanged,
                                        int /*attackTypeOrd*/,
                                        int damageTypeOrd,
                                        float incomingBaseDamage,
                                        float incomingBaseBonus,
                                        float actualDamage,
                                        bool wasEvaded,
                                        bool invulnerableTarget) {
    ++stats_.postDamageReactions;
    const auto damageType = static_cast<CDamageType>(damageTypeOrd);

    // 1. Sleep (BUsl) breaks on damage
    if ((invulnerableTarget || actualDamage != 0.0f) && target.IsAsleep()) {
        target.RemoveBuff(War3ID("BUsl"), sim);
    }

    // 2. Thorns Aura (CBuffThorns) & Spiked Carapace (AUts) melee reflection via virtual contracts
    if (isAttack && !isRanged && attacker && attacker != &target &&
        damageType == CDamageType::NORMAL &&
        (invulnerableTarget ? !wasEvaded : actualDamage != 0.0f)) {
        bool enemy = true;
        if (CPlayer* self = sim.GetPlayer(target.GetPlayerIndex())) {
            if (CPlayer* foe = sim.GetPlayer(attacker->GetPlayerIndex())) {
                enemy = !self->HasAlliance(*foe, CAllianceType::ALLIED);
            }
        }
        const bool resisted = sim.GetGameplayConstants().magicImmuneResistsThorns &&
                              attacker->IsMagicImmune();
        if (enemy && !resisted) {
            float bestPercent = 0.0f;
            for (const auto& buff : target.GetBuffs()) {
                if (buff) bestPercent = std::max(bestPercent, buff->GetReflectPercent());
            }
            if (bestPercent > 0.0f) {
                const float reflected = (incomingBaseDamage + incomingBaseBonus) * bestPercent;
                if (reflected > 0.0f) {
                    attacker->Damage(sim, nullptr, false, false,
                                     CAttackType::SPELLS, CDamageType::DEFENSIVE,
                                     "", reflected, 0.0f);
                }
            }
            if (const CAbility* carapace = target.GetAbility(War3ID("AUts"))) {
                const float reflected = (incomingBaseDamage + incomingBaseBonus) *
                                        carapace->GetReflectPercent();
                if (reflected > 0.0f) {
                    attacker->Damage(sim, nullptr, false, false,
                                     CAttackType::SPELLS, CDamageType::DEFENSIVE,
                                     "", reflected, 0.0f);
                }
            }
        }
    }

    // 3. Vampiric Aura / Item Lifesteal (CBuffLifesteal) via CBuff::GetStealPercent()
    if (isAttack && attacker && attacker != &target && actualDamage > 0.0f && !target.IsBuilding()) {
        bool mechanical = false;
        if (const CUnitType* ut = target.GetUnitType()) {
            const auto& classes = ut->GetClassifications();
            mechanical = classes.find(CUnitClassification::MECHANICAL) != classes.end();
        }
        bool passiveAlly = false;
        if (CPlayer* atkP = sim.GetPlayer(attacker->GetPlayerIndex())) {
            if (CPlayer* vicP = sim.GetPlayer(target.GetPlayerIndex())) {
                passiveAlly = atkP->HasAlliance(*vicP, CAllianceType::PASSIVE);
            }
        }
        const bool resisted = sim.GetGameplayConstants().magicImmuneResistsLeech &&
                              target.IsMagicImmune();
        if (!mechanical && !passiveAlly && !resisted) {
            float bestPercent = 0.0f;
            for (const auto& buff : attacker->GetBuffs()) {
                if (buff) bestPercent = std::max(bestPercent, buff->GetStealPercent());
            }
            if (bestPercent > 0.0f) {
                float healed = attacker->GetLife() + actualDamage * bestPercent;
                if (healed > attacker->GetMaxLife()) healed = attacker->GetMaxLife();
                attacker->SetLife(sim, healed);
            }
        }
    }

    // 4. Bash (AHbh) stun proc via CAbility::GetProcChance() / GetProcDuration()
    if (isAttack && attacker && attacker != &target && actualDamage > 0.0f && !target.IsDead()) {
        if (const CAbility* bash = attacker->GetAbility(War3ID("AHbh"))) {
            if (sim.SimRand01() < bash->GetProcChance()) {
                target.AddBuff(std::make_unique<CBuffStun>(bash->GetProcDuration()), sim);
                sim.FireCombatText(attacker->GetHandleId(), target.GetHandleId(),
                                   CSimulation::CombatTextKind::BASH, 0.0f);
            }
        }
    }

    // 5. Orb / Arrow attack modifier slot
    if (isAttack && attacker && attacker != &target && actualDamage > 0.0f && !target.IsDead()) {
        attacker->FireAttackModifiers(sim, &target, actualDamage);
    }

    // 6. Cleaving Attack (ANca) splash via CAbility::GetSplashFraction() / GetSplashRadius()
    if (isAttack && attacker && attacker != &target && actualDamage > 0.0f && !target.IsDead()) {
        if (const CAbility* cleave = attacker->GetAbility(War3ID("ANca"))) {
            const float splash = actualDamage * cleave->GetSplashFraction();
            const float radius = cleave->GetSplashRadius();
            if (splash > 0.0f) {
                CPlayer* srcPlayer = sim.GetPlayer(attacker->GetPlayerIndex());
                for (auto* unit : sim.GetUnits()) {
                    if (!unit || unit->IsDead() || unit == &target || unit == attacker) continue;
                    CPlayer* tgtPlayer = sim.GetPlayer(unit->GetPlayerIndex());
                    const bool isAlly = (unit->GetPlayerIndex() == attacker->GetPlayerIndex()) ||
                        (srcPlayer && tgtPlayer && (srcPlayer->HasAlliance(*tgtPlayer, CAllianceType::ALLIED) ||
                                                    srcPlayer->HasAlliance(*tgtPlayer, CAllianceType::PASSIVE)));
                    if (isAlly) continue;
                    const float dx = unit->GetX() - target.GetX();
                    const float dy = unit->GetY() - target.GetY();
                    if (dx * dx + dy * dy <= radius * radius) {
                        unit->Damage(sim, attacker, false, false,
                                     CAttackType::SPELLS, CDamageType::NORMAL,
                                     "", splash, 0.0f);
                    }
                }
            }
        }
    }

    // 7. Wind Walk (BOwk) break & backstab bonus damage via CBuff::GetBackstabDamage()
    if (isAttack && attacker && attacker != &target && actualDamage > 0.0f) {
        if (const CBuff* walk = attacker->GetBuff(War3ID("BOwk"))) {
            const float backstab = walk->GetBackstabDamage();
            attacker->RemoveBuff(War3ID("BOwk"), sim);
            if (backstab > 0.0f && !target.IsDead()) {
                target.Damage(sim, attacker, false, false,
                              CAttackType::SPELLS, CDamageType::NORMAL,
                              "", backstab, 0.0f);
            }
        }
    }

    // 8. Pulverize (Awar) AoE slam proc via CAbility::GetProcChance() / GetSplashRadius()
    if (isAttack && attacker && attacker != &target && actualDamage > 0.0f && !target.IsDead()) {
        if (const CAbility* pulv = attacker->GetAbility(War3ID("Awar"))) {
            if (sim.SimRand01() < pulv->GetProcChance()) {
                const float radius = pulv->GetSplashRadius();
                CPlayer* srcPlayer = sim.GetPlayer(attacker->GetPlayerIndex());
                for (auto* unit : sim.GetUnits()) {
                    if (!unit || unit->IsDead() || unit == &target || unit == attacker) continue;
                    CPlayer* tgtPlayer = sim.GetPlayer(unit->GetPlayerIndex());
                    const bool isAlly = (unit->GetPlayerIndex() == attacker->GetPlayerIndex()) ||
                        (srcPlayer && tgtPlayer && (srcPlayer->HasAlliance(*tgtPlayer, CAllianceType::ALLIED) ||
                                                    srcPlayer->HasAlliance(*tgtPlayer, CAllianceType::PASSIVE)));
                    if (isAlly) continue;
                    const float dx = unit->GetX() - target.GetX();
                    const float dy = unit->GetY() - target.GetY();
                    if (dx * dx + dy * dy <= radius * radius) {
                        unit->Damage(sim, attacker, false, false,
                                     CAttackType::SPELLS, CDamageType::NORMAL,
                                     "", actualDamage, 0.0f);
                    }
                }
            }
        }
    }
}

void War3GameModule::OnAwardKillBountyAndXP(CSimulation& sim,
                                            CUnit* killer,
                                            CUnit& victim) {
    const CUnitType* victimType = victim.GetUnitType();
    if (!killer || !victimType) return;

    CPlayer* victimOwner = sim.GetPlayer(victim.GetPlayerIndex());
    CPlayer* killerPlayer = sim.GetPlayer(killer->GetPlayerIndex());
    if (!killerPlayer) return;
    if (victimOwner && killerPlayer->HasAlliance(*victimOwner, CAllianceType::PASSIVE)) return;

    // 1. Gold/lumber bounty dice (base + dice d sides, scaled by bountyMultiplier_)
    if (victimOwner && victimOwner->GetGivesBounty()) {
        int gold = victimType->GetGoldBountyAwardedBase();
        const int goldDice = victimType->GetGoldBountyAwardedDice();
        const int goldSides = victimType->GetGoldBountyAwardedSides();
        for (int i = 0; i < goldDice; ++i) {
            gold += goldSides <= 0 ? 0 : static_cast<int>(sim.SimRand01() * goldSides) + 1;
        }
        gold = static_cast<int>(std::round(static_cast<float>(gold) * bountyMultiplier_));
        if (gold > 0) killerPlayer->AddGold(gold);

        int lumber = victimType->GetLumberBountyAwardedBase();
        const int lumberDice = victimType->GetLumberBountyAwardedDice();
        const int lumberSides = victimType->GetLumberBountyAwardedSides();
        for (int i = 0; i < lumberDice; ++i) {
            lumber += lumberSides <= 0 ? 0 : static_cast<int>(sim.SimRand01() * lumberSides) + 1;
        }
        lumber = static_cast<int>(std::round(static_cast<float>(lumber) * bountyMultiplier_));
        if (lumber > 0) killerPlayer->AddLumber(lumber);

        if (gold > 0 || lumber > 0) {
            ++stats_.bountiesAwarded;
        }
    }

    // 2. Hero XP split (grant tables + heroExpRange + globalExperience fallback, scaled by experienceMultiplier_)
    const auto& gc = sim.GetGameplayConstants();
    if (!gc.buildingKillsGiveExp && killer->IsBuilding()) return;
    const int baseAvailableXp = victim.IsHero()
        ? gc.GetGrantHeroXP(victim.GetHeroLevel() > 0 ? victim.GetHeroLevel() : 1)
        : gc.GetGrantNormalXP(victimType->GetLevel());
    const int availableXp = static_cast<int>(std::round(static_cast<float>(baseAvailableXp) * experienceMultiplier_));
    if (availableXp <= 0) return;

    std::vector<CUnit*> receivers;
    const float range = static_cast<float>(gc.heroExpRange);
    const float rangeSq = range * range;
    for (CUnit* other : sim.GetUnits()) {
        if (!other || other->IsDead() || !other->IsHero()) continue;
        CPlayer* ally = sim.GetPlayer(other->GetPlayerIndex());
        if (!ally || !killerPlayer->HasAlliance(*ally, CAllianceType::SHARED_XP)) continue;
        const float dx = other->GetX() - victim.GetX();
        const float dy = other->GetY() - victim.GetY();
        if (dx * dx + dy * dy > rangeSq) continue;
        receivers.push_back(other);
    }
    if (receivers.empty() && gc.globalExperience) {
        for (int pi = 0; pi < sim.GetPlayerCount(); ++pi) {
            CPlayer* ally = sim.GetPlayer(pi);
            if (!ally || !killerPlayer->HasAlliance(*ally, CAllianceType::SHARED_XP)) continue;
            for (CUnit* hero : ally->GetHeroes()) {
                if (hero && !hero->IsDead()) receivers.push_back(hero);
            }
        }
    }
    if (receivers.empty()) return;

    bool anyXpAwarded = false;
    for (CUnit* hero : receivers) {
        CAbility* heroAbility = hero->GetAbility(War3ID("AHer"));
        if (!heroAbility) continue;
        const int share = static_cast<int>(availableXp * (1.0f / static_cast<float>(receivers.size())) *
                                           gc.GetHeroFactorXp(hero->GetHeroLevel()));
        if (share > 0) {
            heroAbility->AddXP(sim, hero, share);
            anyXpAwarded = true;
        }
    }
    if (anyXpAwarded) {
        ++stats_.heroXpDistributions;
    }
}

int War3GameModule::GetAbilityAutocastKind(uint32_t abilityRawcode) const {
    auto it = customAutocastKinds_.find(abilityRawcode);
    if (it != customAutocastKinds_.end()) {
        return it->second;
    }
    return static_cast<int>(GetAutocastKind(War3ID(abilityRawcode)));
}

void War3GameModule::RegisterAutocastAbility(War3ID abilityCode, int autocastKindOrd) {
    if (abilityCode.value == 0) return;
    customAutocastKinds_[abilityCode.value] = autocastKindOrd;
    ++stats_.customAutocastRegistered;
}

void War3GameModule::OnSnapBuildPosition(const CUnitType* buildType,
                                         float& inOutX,
                                         float& inOutY) const {
    CAbilityBuild::RoundBuildPositionForType(buildType, inOutX, inOutY);
}

bool War3GameModule::IsBuildLocationObstructed(CSimulation& sim,
                                               float x,
                                               float y,
                                               int playerIndex,
                                               uint32_t buildingTypeRawcode) const {
    return CAbilityBuild::IsBuildLocationObstructed(sim, x, y, playerIndex, War3ID(buildingTypeRawcode));
}

bool War3GameModule::OnTryUnitIdleAutocast(CSimulation& sim, CUnit& unit) {
    ++stats_.autocastEvaluations;
    if (unit.IsDead() || unit.IsBuilding() || unit.IsPaused()) return false;
    if (unit.HasQueuedOrders() || unit.GetCurrentBehavior() != nullptr) return false;
    if (unit.IsSilenced() || unit.IsHexed() || unit.IsAsleep()) return false;

    CPlayer* self = sim.GetPlayer(unit.GetPlayerIndex());
    const CUnitType* uType = unit.GetUnitType();
    const War3ID defaultAuto = uType ? uType->GetDefaultAutocastAbility() : War3ID(0u);

    for (CAbility* ab : unit.GetAbilities()) {
        CAbility* spell = (ab && ab->IsSpellAbility()) ? ab : nullptr;
        if (!spell || spell->IsDisabled() || spell->IsOnCooldown()) continue;
        auto kind = static_cast<CAutocastKind>(GetAbilityAutocastKind(spell->GetAlias().value));
        if (kind == CAutocastKind::NONE) {
            kind = static_cast<CAutocastKind>(GetAbilityAutocastKind(spell->GetCode().value));
        }
        if (kind == CAutocastKind::NONE) continue;
        if (!spell->IsAutocastActive() && defaultAuto != spell->GetAlias() &&
            defaultAuto != spell->GetCode()) continue;
        if (static_cast<int>(unit.GetMana()) < spell->GetManaCost()) continue;
        const int orderId = spell->GetBaseOrderId();
        if (orderId <= 0) continue;
        if (!spell->CheckCanUse(sim, &unit, orderId)) continue;

        const float range = spell->GetCastRange() + 400.0f;
        const float rangeSq = range * range;
        CUnit* best = nullptr;

        if (kind == CAutocastKind::LOWEST_HP_ALLY) {
            float bestFrac = 1.0f;
            for (CUnit* other : sim.GetUnits()) {
                if (!other || other->IsDead() || other->IsHidden() ||
                    other->IsLoadedAboard() || other->IsBuilding()) continue;
                CPlayer* foe = sim.GetPlayer(other->GetPlayerIndex());
                const bool ally = (other == &unit) ||
                    (self && foe && self->HasAlliance(*foe, CAllianceType::PASSIVE));
                if (!ally) continue;
                if (other->GetLife() >= other->GetMaxLife()) continue;
                const float dx = other->GetX() - unit.GetX();
                const float dy = other->GetY() - unit.GetY();
                if (dx * dx + dy * dy > rangeSq) continue;
                if (!spell->CheckCanTarget(sim, &unit, orderId, other)) continue;
                const float frac = other->GetLife() / other->GetMaxLife();
                if (!best || frac < bestFrac) {
                    best = other;
                    bestFrac = frac;
                }
            }
        } else {
            float bestDist = rangeSq;
            for (CUnit* other : sim.GetUnits()) {
                if (!other || other->IsDead() || other->IsHidden() ||
                    other->IsLoadedAboard() || other == &unit) continue;
                CPlayer* foe = sim.GetPlayer(other->GetPlayerIndex());
                if (self && foe && self->HasAlliance(*foe, CAllianceType::PASSIVE)) continue;
                const float dx = other->GetX() - unit.GetX();
                const float dy = other->GetY() - unit.GetY();
                const float d2 = dx * dx + dy * dy;
                if (d2 > rangeSq || (best && d2 >= bestDist)) continue;
                if (!spell->CheckCanTarget(sim, &unit, orderId, other)) continue;
                best = other;
                bestDist = d2;
            }
        }
        if (best) {
            unit.IssueOrder(std::make_unique<COrderTargetWidget>(orderId, best));
            unit.PollNextOrderBehavior(sim);
            if (unit.GetCurrentBehavior() != nullptr) {
                ++stats_.autocastOrdersIssued;
                return true;
            }
            return false;
        }
    }
    return false;
}

void War3GameModule::OnApplyUnitItemBonus(CSimulation& sim,
                                          CUnit& unit,
                                          int bonusTypeOrd,
                                          int signedDelta) {
    ++stats_.itemBonusesApplied;
    const auto bonusType = static_cast<CItemBonusType>(bonusTypeOrd);
    auto findHeroAbility = [](CUnit& u) -> CAbility* {
        if (!u.IsHero()) return nullptr;
        return u.GetAbility(War3ID("AHer"));
    };

    switch (bonusType) {
        case CItemBonusType::STRENGTH:
            if (auto* hero = findHeroAbility(unit)) {
                hero->AddStrengthBonus(sim, &unit, signedDelta);
            } else {
                unit.AddBonusMaxLife(static_cast<float>(signedDelta) * 25.0f);
            }
            break;
        case CItemBonusType::AGILITY:
            if (auto* hero = findHeroAbility(unit)) {
                hero->AddAgilityBonus(sim, &unit, signedDelta);
            } else {
                unit.AddBonusArmor(signedDelta >= 0 ? (signedDelta * 33 + 50) / 100
                                                    : -((-signedDelta * 33 + 50) / 100));
            }
            break;
        case CItemBonusType::INTELLIGENCE:
            if (auto* hero = findHeroAbility(unit)) {
                hero->AddIntelligenceBonus(sim, &unit, signedDelta);
            } else {
                unit.AddBonusMaxMana(signedDelta * 15);
            }
            break;
        case CItemBonusType::DAMAGE:
            unit.AddBonusDamage(signedDelta);
            break;
        case CItemBonusType::ARMOR:
            unit.AddBonusArmor(signedDelta);
            break;
        case CItemBonusType::ATTACK_SPEED:
            unit.AddStackingAttackSpeed(static_cast<float>(signedDelta) / 100.0f);
            break;
        case CItemBonusType::MAX_LIFE:
            unit.AddBonusMaxLife(static_cast<float>(signedDelta));
            break;
        case CItemBonusType::MAX_MANA:
            unit.AddBonusMaxMana(signedDelta);
            break;
        case CItemBonusType::MOVE_SPEED:
            unit.AddBonusSpeed(signedDelta);
            break;
        case CItemBonusType::LIFE_REGEN:
            unit.AddBonusLifeRegen(static_cast<float>(signedDelta));
            break;
        case CItemBonusType::MANA_REGEN:
            unit.AddBonusManaRegen(static_cast<float>(signedDelta));
            break;
        case CItemBonusType::SIGHT_RADIUS:
        case CItemBonusType::NONE:
        default:
            break;
    }
}

bool War3GameModule::IsCarriableHero(const CSimulation& sim, CUnit& unit) const {
    if (unit.IsHidden()) return false;
    const CUnitType* type = unit.GetUnitType();
    if (!type || !type->IsHero()) return false;
    CPlayer* owner = sim.GetPlayer(unit.GetPlayerIndex());
    if (!owner || owner->GetController() != CMapControl::USER) return false;
    for (CAbility* ab : unit.GetAbilities()) {
        if (ab && ab->GetCategory() == CAbilityCategory::HERO) return true;
    }
    return false;
}

bool War3GameModule::OnSnapshotCarriedHero(const CSimulation& sim,
                                           CUnit& unit,
                                           CarriedHero& outCarried) const {
    outCarried = CarriedHero{};
    if (!IsCarriableHero(sim, unit)) return false;

    CAbility* hero = nullptr;
    for (CAbility* ab : unit.GetAbilities()) {
        if (ab && ab->GetCategory() == CAbilityCategory::HERO) {
            hero = ab;
            break;
        }
    }
    if (!hero) return false;

    outCarried.typeId = unit.GetTypeId();
    outCarried.ownerPlayer = unit.GetPlayerIndex();
    outCarried.level = hero->GetHeroLevel();
    outCarried.xp = hero->GetXP();
    outCarried.skillPoints = hero->GetSkillPoints();
    outCarried.properName = hero->GetProperName();
    const auto& learnable = hero->GetLearnableSkills();
    for (CAbility* ab : unit.GetAbilities()) {
        if (!ab || learnable.count(ab->GetCode()) == 0) continue;
        outCarried.learnedSkills.emplace_back(ab->GetCode(), ab->GetLevel());
    }
    if (CAbility* inv = unit.GetAbility(War3ID("AInv"))) {
        for (int slot = 0; slot < inv->GetItemCapacity(); ++slot) {
            CItem* item = inv->GetItem(slot);
            if (!item) continue;
            CarriedHero::CarriedItem ci;
            ci.typeId = item->GetTypeId();
            ci.charges = item->GetCharges();
            outCarried.items.push_back(ci);
        }
    }
    return true;
}

CUnit* War3GameModule::OnRespawnCarriedHero(CSimulation& sim,
                                            const CarriedHero& carried,
                                            float x,
                                            float y) {
    if (carried.typeId.value == 0) return nullptr;
    CPlayer* owner = sim.GetPlayer(carried.ownerPlayer);
    if (!owner) return nullptr;
    if (!sim.GetUnitData() || !sim.GetUnitData()->GetUnitType(carried.typeId)) {
        return nullptr;
    }
    CUnit* hero = sim.CreateUnit(carried.typeId, carried.ownerPlayer, x, y, 0.0f);
    if (!hero) return nullptr;

    CAbility* heroAb = nullptr;
    for (CAbility* ab : hero->GetAbilities()) {
        if (ab && ab->GetCategory() == CAbilityCategory::HERO) {
            heroAb = ab;
            break;
        }
    }
    if (heroAb) {
        heroAb->SetHeroLevel(sim, hero, carried.level);
        heroAb->SetXP(sim, hero, carried.xp);
        heroAb->SetSkillPoints(carried.skillPoints);
        if (!carried.properName.empty()) heroAb->SetProperName(carried.properName);
    }

    for (const auto& [code, level] : carried.learnedSkills) {
        if (level <= 0) continue;
        if (CAbility* existing = hero->GetAbility(code)) {
            existing->SetLevel(level);
            continue;
        }
        auto created = AbilityFactory::CreateAbility(sim.AllocateHandleId(), code);
        if (!created) continue;
        created->SetLevel(level);
        hero->AddAbility(sim, std::move(created));
    }

    if (!carried.items.empty()) {
        if (CAbility* inv = hero->GetAbility(War3ID("AInv"))) {
            for (const auto& ci : carried.items) {
                CItem* item = sim.CreateItem(ci.typeId, x, y);
                if (!item) continue;
                item->SetCharges(ci.charges);
                if (!inv->PickUpItem(sim, hero, item)) {
                    break;
                }
            }
        } else {
            for (const auto& ci : carried.items) {
                sim.CreateItem(ci.typeId, x, y);
            }
        }
    }
    ++stats_.heroesCarriedOver;
    return hero;
}


bool War3GameModule::OnDiscoverAIRacePlan(const CSimulation& sim,
                                          int playerIndex,
                                          AIRacePlan& outPlan) const {
    return DiscoverRaceBuildPlan(sim, playerIndex, outPlan);
}

void War3GameModule::RegisterCustomOrder(const std::string& orderName,
                                         int orderId,
                                         uint32_t abilityRawcode,
                                         int secondaryOrderId) {
    if (!orderName.empty() && orderId != 0) {
        RegisterCustomOrderString(orderName, orderId);
    }
    if (abilityRawcode != 0) {
        RegisterAbilityOrderMapping(War3ID(abilityRawcode), orderId, secondaryOrderId);
    }
    ++stats_.customOrdersRegistered;
}

void War3GameModule::RegisterCustomAura(War3ID code,
                                        float radius,
                                        bool affectsAllies,
                                        bool affectsSelf,
                                        bool enemyOnly,
                                        const std::vector<CAuraBonus>& bonuses) {
    if (code.value == 0) return;
    AbilityFactory::RegisterModuleCreator(
        kModuleId,
        code,
        [radius, affectsAllies, affectsSelf, enemyOnly, bonuses](int handleId, War3ID abilityCode) -> std::unique_ptr<CAbility> {
            auto aura = std::make_unique<CAbilityAura>(handleId, abilityCode);
            aura->SetRadius(radius);
            aura->SetAffectsAllies(affectsAllies);
            aura->SetAffectsSelf(affectsSelf);
            aura->SetEnemyOnly(enemyOnly);
            for (const auto& b : bonuses) {
                aura->AddBonus(b);
            }
            return aura;
        });
    ++stats_.customAurasRegistered;
}

bool War3GameModule::UnregisterCustomAura(War3ID code) {
    if (code.value == 0) return false;
    AbilityFactory::UnregisterCreator(code);
    return true;
}


bool War3GameModule::RegisterCustomItemArchetype(CSimulation& sim,
                                                 War3ID itemId,
                                                 const std::string& name,
                                                 int goldCost,
                                                 int lumberCost,
                                                 int charges,
                                                 const std::vector<War3ID>& abilityList) {
    CItemData* itemData = sim.GetItemData();
    if (!itemData || itemId.value == 0) return false;
    auto type = std::make_unique<CItemType>();
    type->id = itemId;
    type->name = name;
    type->legacyName = name;
    type->goldCost = goldCost;
    type->lumberCost = lumberCost;
    type->charges = charges;
    type->perishable = (charges > 0);
    type->droppable = true;
    type->pawnable = true;
    type->sellable = true;
    type->abilityList = abilityList;
    itemData->RegisterItemType(itemId, std::move(type));
    ++stats_.customDataArchetypesRegistered;
    return true;
}

bool War3GameModule::RegisterCustomUpgradeArchetype(CSimulation& sim,
                                                    War3ID upgradeId,
                                                    const std::string& name,
                                                    int maxLevel,
                                                    int goldBase,
                                                    int goldMod,
                                                    const std::string& effectCode,
                                                    float effectBase,
                                                    float effectMod) {
    CUpgradeData* upgradeData = sim.GetUpgradeData();
    if (!upgradeData || upgradeId.value == 0) return false;
    auto upg = std::make_unique<CUpgradeType>();
    upg->id = upgradeId;
    upg->name = name;
    upg->maxLevel = std::max(1, maxLevel);
    upg->goldCost = goldBase;
    for (int l = 1; l <= upg->maxLevel; ++l) {
        upg->goldPerLevel.push_back(goldBase + goldMod * (l - 1));
        upg->lumberPerLevel.push_back(0);
        upg->timePerLevel.push_back(10);
    }
    if (!effectCode.empty()) {
        CUpgradeEffectSpec spec;
        spec.effectCode = effectCode;
        spec.base = effectBase;
        spec.mod = effectMod;
        upg->effects.push_back(std::move(spec));
    }
    upgradeData->RegisterUpgrade(upgradeId, std::move(upg));
    ++stats_.customDataArchetypesRegistered;
    return true;
}

bool War3GameModule::RegisterCustomAbilityDef(CSimulation& sim,
                                              War3ID abilityId,
                                              const std::string& name,
                                              int levels,
                                              float cooldown,
                                              int manaCost,
                                              float range,
                                              float area,
                                              float duration) {
    CAbilityData* abilityData = sim.GetAbilityData();
    if (!abilityData || abilityId.value == 0) return false;
    auto def = std::make_unique<CAbilityDefFull>();
    def->id = abilityId;
    def->name = name;
    def->legacyName = name;
    def->levels = std::max(1, levels);
    def->cooldown = cooldown;
    def->manaCost = manaCost;
    def->range = range;
    def->area = area;
    def->duration = duration;
    for (int l = 0; l < def->levels; ++l) {
        def->cooldowns.push_back(cooldown);
        def->manaCosts.push_back(manaCost);
        def->ranges.push_back(range);
        def->areas.push_back(area);
        def->durations.push_back(duration);
    }
    abilityData->RegisterAbility(abilityId, std::move(def));
    ++stats_.customDataArchetypesRegistered;
    return true;
}

void War3GameModule::RegisterCustomBuffCreator(War3ID buffCode, BuffCreatorFn creator) {
    if (buffCode.value == 0 || !creator) return;
    customBuffCreators_[buffCode.value] = std::move(creator);
    ++stats_.customBuffsRegistered;
}

void War3GameModule::RegisterCustomBuffSpec(const DataDrivenBuffSpec& spec) {
    if (spec.id.value == 0) return;
    customBuffSpecs_[spec.id.value] = spec;
    ++stats_.customBuffsRegistered;
}

bool War3GameModule::UnregisterCustomBuff(War3ID buffCode) {
    if (buffCode.value == 0) return false;
    const size_t c1 = customBuffCreators_.erase(buffCode.value);
    const size_t c2 = customBuffSpecs_.erase(buffCode.value);
    return (c1 + c2) > 0;
}

bool War3GameModule::HasCustomBuff(War3ID buffCode) const {
    return customBuffCreators_.count(buffCode.value) > 0 ||
           customBuffSpecs_.count(buffCode.value) > 0;
}

CBuff* War3GameModule::CreateTimedLifeBuff(uint32_t buffRawcode,
                                           float duration) const {
    return new CBuffTimedLife(War3ID(buffRawcode), duration);
}

CBuff* War3GameModule::CreateBuffByCode(uint32_t buffRawcode,
                                        float duration,
                                        int casterHandleId) const {
    auto it = customBuffCreators_.find(buffRawcode);
    if (it != customBuffCreators_.end() && it->second) {
        return it->second(duration, casterHandleId).release();
    }
    const War3ID code(buffRawcode);
    const float dur = duration > 0.0f ? duration : 10.0f;
    if (code == War3ID("Bstn")) return new CBuffStun(dur);
    if (code == War3ID("BUsl")) return new CBuffSleep(dur);
    if (code == War3ID("BNsi")) return new CBuffSilence(dur);
    if (code == War3ID("BOhx")) return new CBuffHex(dur);
    if (code == War3ID("Bply")) return new CBuffPolymorph(dur);
    if (code == War3ID("Bdsh")) return new CBuffDivineShield(dur);
    if (code == War3ID("Bdef")) return new CBuffDefend(dur);
    if (code == War3ID("Bena")) return new CBuffEnsnare(dur);
    if (code == War3ID("BNms")) return new CBuffManaShield(dur);
    if (code == War3ID("BNss")) return new CBuffSpellShield(dur);
    if (code == War3ID("Brsk")) return new CBuffResistant();
    if (code == War3ID("BTLF")) return new CBuffTimedLife(code, dur);
    return nullptr;
}

bool War3GameModule::TryApplyCustomBuffByCode(CSimulation& sim,
                                              CUnit& target,
                                              uint32_t buffRawcode,
                                              float durationOverride,
                                              int casterHandleId) {
    auto specIt = customBuffSpecs_.find(buffRawcode);
    if (specIt != customBuffSpecs_.end()) {
        DataDrivenBuffSpec spec = specIt->second;
        if (durationOverride > 0.0f) spec.duration = durationOverride;
        if (casterHandleId >= 0) spec.casterHandleId = casterHandleId;
        if (target.ApplyDataDrivenBuff(spec, sim)) {
            ++stats_.customBuffsApplied;
            return true;
        }
        return false;
    }
    if (CBuff* rawBuff = CreateBuffByCode(buffRawcode, durationOverride, casterHandleId)) {
        target.AddBuff(std::unique_ptr<CBuff>(rawBuff), sim);
        ++stats_.customBuffsApplied;
        return true;
    }
    return false;
}


// ============================================================================
// Phase 23: Engine-Level Pluggable Projectile & Unit Behavior State Machine
// ============================================================================

void War3GameModule::RegisterCustomProjectileSpec(const CustomProjectileSpec& spec) {
    if (spec.code.value == 0) return;
    customProjectileSpecs_[spec.code.value] = spec;
    ++stats_.customProjectilesRegistered;
}

void War3GameModule::RegisterCustomProjectileImpact(War3ID projCode, ProjectileImpactFn fn) {
    if (projCode.value == 0 || !fn) return;
    customProjectileImpacts_[projCode.value] = std::move(fn);
    ++stats_.customProjectilesRegistered;
}

bool War3GameModule::UnregisterCustomProjectile(War3ID projCode) {
    if (projCode.value == 0) return false;
    const size_t c1 = customProjectileSpecs_.erase(projCode.value);
    const size_t c2 = customProjectileImpacts_.erase(projCode.value);
    return (c1 + c2) > 0;
}

bool War3GameModule::HasCustomProjectile(War3ID projCode) const {
    return customProjectileSpecs_.count(projCode.value) > 0 ||
           customProjectileImpacts_.count(projCode.value) > 0;
}

bool War3GameModule::OnProjectilePreImpact(CSimulation& sim,
                                           CProjectile& proj,
                                           CUnit* source,
                                           CWidget* target,
                                           bool& outTerminate) {
    ++stats_.projectilePreImpactsEvaluated;
    outTerminate = true;
    auto* victim = dynamic_cast<CUnit*>(target);
    if (!victim) return false;

    if (proj.checkEffectReaction) {
        if (!victim->CheckSpellEffectReaction(sim, source)) {
            outTerminate = true;
            return true;
        }
    }

    // WarCraft 3 Dragon Scale (`BIdd`) pierce projectile reflection:
    // reflects a pierce projectile once back at the attacker.
    if (!proj.pierceReflected && proj.attackType == CAttackType::PIERCE) {
        if (victim->HasBuff(War3ID("BIdd")) && source &&
            source->GetHandleId() != victim->GetHandleId()) {
            proj.pierceReflected = true;
            proj.targetHandleId = source->GetHandleId();
            proj.attackerHandleId = victim->GetHandleId();
            proj.traveled = 0.0f;
            proj.fx = proj.x;
            proj.fy = proj.y;
            outTerminate = false;
            return true;
        }
    }

    return false;
}

bool War3GameModule::OnProjectileCustomImpact(CSimulation& sim,
                                              CProjectile& proj,
                                              CUnit* source,
                                              CWidget* target,
                                              float impactX,
                                              float impactY) {
    bool handled = false;
    auto fnIt = customProjectileImpacts_.find(proj.projectileTypeId.value);
    if (fnIt != customProjectileImpacts_.end() && fnIt->second) {
        fnIt->second(sim, proj, source, target, impactX, impactY);
        handled = true;
    }

    auto specIt = customProjectileSpecs_.find(proj.projectileTypeId.value);
    if (specIt != customProjectileSpecs_.end()) {
        const CustomProjectileSpec& spec = specIt->second;
        if (spec.impactBuffCode.value != 0) {
            if (auto* victim = dynamic_cast<CUnit*>(target)) {
                TryApplyCustomBuffByCode(sim, *victim, spec.impactBuffCode.value,
                                         spec.impactBuffDuration,
                                         source ? source->GetHandleId() : -1);
            }
        }
        if (spec.onImpact) {
            spec.onImpact(sim, source, target, impactX, impactY);
        }
        handled = true;
    }
    return handled;
}

CUnit* War3GameModule::OnFindProjectileBounceTarget(CSimulation& sim,
                                                    const CProjectile& proj,
                                                    CUnit* source,
                                                    CWidget* currentTarget) {
    if (!source) return nullptr;
    CPlayer* ownerPlayer = sim.GetPlayer(source->GetPlayerIndex());
    const int currentHandle = currentTarget ? currentTarget->GetHandleId() : proj.targetHandleId;
    CUnit* best = nullptr;
    float bestDistSq = proj.bounceRange * proj.bounceRange;

    for (CUnit* other : sim.GetUnits()) {
        if (!other || other->IsDead() || other == source) continue;
        if (other->GetHandleId() == currentHandle) continue;

        bool isEnemy = (other->GetPlayerIndex() != source->GetPlayerIndex());
        if (ownerPlayer) {
            if (CPlayer* otherPlayer = sim.GetPlayer(other->GetPlayerIndex())) {
                isEnemy = !ownerPlayer->HasAlliance(*otherPlayer, CAllianceType::ALLIED);
            } else {
                isEnemy = false;
            }
        }
        if (!isEnemy) continue;

        const float ox = other->GetX() - proj.x;
        const float oy = other->GetY() - proj.y;
        const float dSq = ox * ox + oy * oy;
        if (dSq < bestDistSq) {
            bestDistSq = dSq;
            best = other;
        }
    }
    return best;
}

bool War3GameModule::SpawnCustomProjectile(CSimulation& sim,
                                           uint32_t projRawcode,
                                           CUnit* source,
                                           CWidget* target,
                                           float targetX,
                                           float targetY) {
    auto it = customProjectileSpecs_.find(projRawcode);
    if (it == customProjectileSpecs_.end()) return false;
    const CustomProjectileSpec& spec = it->second;

    CProjectile shot;
    shot.projectileTypeId = spec.code;
    shot.attackerHandleId = source ? source->GetHandleId() : -1;
    shot.x = source ? source->GetX() : 0.0f;
    shot.y = source ? source->GetY() : 0.0f;
    shot.fx = shot.x;
    shot.fy = shot.y;
    shot.speed = spec.speed;
    shot.damage = spec.damage;
    shot.attackType = spec.attackType;
    shot.damageType = spec.damageType;
    shot.isAttack = spec.isAttack;
    shot.isRanged = spec.isRanged;
    shot.checkEffectReaction = spec.checkEffectReaction;
    shot.customImpactOnly = spec.customImpactOnly;
    shot.splashFull = spec.splashFull;
    shot.splashHalf = spec.splashHalf;
    shot.splashQuarter = spec.splashQuarter;
    shot.splashFactorHalf = spec.splashFactorHalf;
    shot.splashFactorQuarter = spec.splashFactorQuarter;
    shot.bouncesLeft = spec.bounces;
    shot.bounceRange = spec.bounceRange;
    shot.bounceLossFactor = spec.bounceLossFactor;
    shot.spillDistance = spec.spillDistance;
    shot.spillRadius = spec.spillRadius;

    const float aimX = (!spec.groundTarget && target) ? target->GetX() : targetX;
    const float aimY = (!spec.groundTarget && target) ? target->GetY() : targetY;
    if (spec.groundTarget || !target) {
        shot.groundTarget = true;
        shot.targetHandleId = -1;
        shot.tx = aimX;
        shot.ty = aimY;
    } else {
        shot.groundTarget = false;
        shot.targetHandleId = target->GetHandleId();
    }

    const float dx = aimX - shot.x;
    const float dy = aimY - shot.y;
    shot.maxTravel = std::sqrt(dx * dx + dy * dy) + 512.0f;
    sim.SpawnProjectile(std::move(shot));
    ++stats_.customProjectilesSpawned;
    return true;
}

void War3GameModule::RegisterCustomBehaviorCreator(int orderId, BehaviorCreatorFn fn) {
    if (orderId == 0 || !fn) return;
    customBehaviorCreators_[orderId] = std::move(fn);
    ++stats_.customBehaviorsRegistered;
}

void War3GameModule::RegisterCustomBehaviorSpec(const CustomBehaviorSpec& spec) {
    if (spec.orderId == 0) return;
    customBehaviorSpecs_[spec.orderId] = spec;
    ++stats_.customBehaviorsRegistered;
}

bool War3GameModule::UnregisterCustomBehavior(int orderId) {
    if (orderId == 0) return false;
    const size_t c1 = customBehaviorCreators_.erase(orderId);
    const size_t c2 = customBehaviorSpecs_.erase(orderId);
    return (c1 + c2) > 0;
}

bool War3GameModule::HasCustomBehavior(int orderId) const {
    return customBehaviorCreators_.count(orderId) > 0 ||
           customBehaviorSpecs_.count(orderId) > 0;
}

CBehavior* War3GameModule::CreateUnitBehavior(CBehaviorCategory category) const {
    auto ovIt = customCategoryBehaviorOverrides_.find(static_cast<uint8_t>(category));
    if (ovIt != customCategoryBehaviorOverrides_.end() && ovIt->second) {
        return ovIt->second();
    }
    class CBehaviorModuleStop final : public CBehavior {
    public:
        CBehaviorCategory GetCategory() const override { return CBehaviorCategory::STOP; }
        void Begin(CSimulation&) override { if (unit_) unit_->ClearOrders(); }
        bool Update(CSimulation&) override { return true; }
    };
    class CBehaviorModuleHold final : public CBehavior {
    public:
        CBehaviorCategory GetCategory() const override { return CBehaviorCategory::HOLD_POSITION; }
        bool Update(CSimulation&) override { return false; }
    };
    if (category == CBehaviorCategory::STOP) return new CBehaviorModuleStop();
    if (category == CBehaviorCategory::HOLD_POSITION) return new CBehaviorModuleHold();
    if (category == CBehaviorCategory::STUN) return CreateStunBehavior(0.0f);
    return nullptr;
}

void War3GameModule::RegisterCategoryBehaviorOverride(CBehaviorCategory category, CategoryBehaviorFactoryFn fn) {
    if (!fn) return;
    customCategoryBehaviorOverrides_[static_cast<uint8_t>(category)] = std::move(fn);
    ++stats_.customBehaviorsRegistered;
}

bool War3GameModule::UnregisterCategoryBehaviorOverride(CBehaviorCategory category) {
    return customCategoryBehaviorOverrides_.erase(static_cast<uint8_t>(category)) > 0;
}

bool War3GameModule::HasCategoryBehaviorOverride(CBehaviorCategory category) const {
    return customCategoryBehaviorOverrides_.count(static_cast<uint8_t>(category)) > 0;
}

void War3GameModule::RegisterCustomDropoffUnitType(War3ID unitTypeId, bool acceptsGold, bool acceptsLumber) {
    if (unitTypeId.value == 0) return;
    customDropoffTypes_[unitTypeId.value] = CustomDropoffRule{acceptsGold, acceptsLumber};
    ++stats_.customDropoffsRegistered;
}

bool War3GameModule::UnregisterCustomDropoffUnitType(War3ID unitTypeId) {
    return customDropoffTypes_.erase(unitTypeId.value) > 0;
}

bool War3GameModule::HasCustomDropoffUnitType(War3ID unitTypeId) const {
    return customDropoffTypes_.count(unitTypeId.value) > 0;
}

bool War3GameModule::CanUnitAcceptResourceDropoff(const CUnit& dropoffUnit,
                                                  int resourceTypeOrd) const {
    const auto resType = static_cast<CResourceType>(resourceTypeOrd);
    if (resType == CResourceType::NONE) return false;
    auto it = customDropoffTypes_.find(dropoffUnit.GetTypeId().value);
    if (it != customDropoffTypes_.end()) {
        if (resType == CResourceType::GOLD && it->second.acceptsGold) return true;
        if (resType == CResourceType::LUMBER && it->second.acceptsLumber) return true;
    }
    if (const CAbility* ret = dropoffUnit.GetAbility(War3ID("Aret"))) {
        if (ret->AcceptsResourceDropoff(resType)) return true;
    }
    for (const CAbility* ab : dropoffUnit.GetAbilities()) {
        if (ab && ab->AcceptsResourceDropoff(resType)) return true;
    }
    return false;
}

CUnit* War3GameModule::OnFindNearestResourceDropoff(CSimulation& sim,
                                                    const CUnit& worker,
                                                    int resourceTypeOrd) {
    ++stats_.resourceDropoffQueries;
    CUnit* nearest = nullptr;
    float bestDistSq = std::numeric_limits<float>::max();
    for (CUnit* other : sim.GetUnits()) {
        if (!other || other->IsDead() || other == &worker) continue;
        if (other->GetPlayerIndex() != worker.GetPlayerIndex()) continue;
        if (other->IsConstructing()) continue;
        if (!CanUnitAcceptResourceDropoff(*other, resourceTypeOrd)) continue;
        const float dx = other->GetX() - worker.GetX();
        const float dy = other->GetY() - worker.GetY();
        const float dSq = dx * dx + dy * dy;
        if (dSq < bestDistSq) {
            bestDistSq = dSq;
            nearest = other;
        }
    }
    return nearest;
}

CWidget* War3GameModule::OnFindNearestHarvestTarget(CSimulation& sim,
                                                    const CUnit& worker,
                                                    int resourceTypeOrd,
                                                    float refX,
                                                    float refY) {
    const auto resType = static_cast<CResourceType>(resourceTypeOrd);
    if (resType == CResourceType::LUMBER) {
        return MoveIntoRange::FindNearestLivingTree(sim, refX, refY);
    }
    if (resType == CResourceType::GOLD) {
        CUnit* nearest = nullptr;
        float bestDistSq = std::numeric_limits<float>::max();
        for (CUnit* other : sim.GetUnits()) {
            if (!other || other->IsDead() || other == &worker) continue;
            const CAbility* mine = other->GetGoldMineAbility();
            if (!mine || mine->GetGold() <= 0) continue;
            const float dx = other->GetX() - refX;
            const float dy = other->GetY() - refY;
            const float dSq = dx * dx + dy * dy;
            if (dSq < bestDistSq) {
                bestDistSq = dSq;
                nearest = other;
            }
        }
        return nearest;
    }
    return nullptr;
}

CBehavior* War3GameModule::CreateStunBehavior(float duration) const {
    class CBehaviorModuleStun final : public CBehavior {
    public:
        explicit CBehaviorModuleStun(float d) : duration_(d), remaining_(d) {}
        CBehaviorCategory GetCategory() const override { return CBehaviorCategory::STUN; }
        bool IsInterruptable() const override { return false; }
        void Begin(CSimulation&) override { remaining_ = duration_; }
        void End(CSimulation&) override { remaining_ = 0.0f; }
        bool Update(CSimulation& sim) override {
            remaining_ -= sim.GetSimStepTime();
            return remaining_ <= 0.0f;
        }
        void SetDuration(float d) override { duration_ = d; }
        float GetRemaining() const override { return remaining_; }
        void SetRemaining(float r) override { remaining_ = r; }
    private:
        float duration_ = 0.0f;
        float remaining_ = 0.0f;
    };
    return new CBehaviorModuleStun(duration);
}

CBehavior* War3GameModule::TryCreateCustomOrderBehavior(CSimulation& sim,
                                                        CUnit* unit,
                                                        const COrder& order) {
    const int orderId = order.GetOrderId();
    auto creatorIt = customBehaviorCreators_.find(orderId);
    if (creatorIt != customBehaviorCreators_.end() && creatorIt->second) {
        ++stats_.customBehaviorsDispatched;
        return creatorIt->second(sim, unit, order);
    }

    auto specIt = customBehaviorSpecs_.find(orderId);
    if (specIt == customBehaviorSpecs_.end()) return nullptr;

    class CBehaviorDataDriven final : public CBehavior {
    public:
        CBehaviorDataDriven(const CustomBehaviorSpec& spec, const COrder& ord)
            : spec_(spec)
            , targetKind_(ord.GetTargetKind())
            , target_(ord.GetTarget())
            , targetHandle_(ord.GetTarget() ? ord.GetTarget()->GetHandleId() : -1)
            , tx_(ord.GetX())
            , ty_(ord.GetY()) {}

        CBehaviorCategory GetCategory() const override { return spec_.category; }
        War3ID GetCustomBehaviorCode() const override { return spec_.code; }
        bool IsInterruptable() const override { return spec_.interruptable; }
        bool TracksPropellingState() const override { return true; }
        bool IsPropelling() const override { return propelling_; }
        bool IsMovingToWorkSite() const override { return movingToRange_ && spec_.category == CBehaviorCategory::BUILD; }
        bool IsActivelyWorking() const override { return !movingToRange_ && (spec_.category == CBehaviorCategory::BUILD || spec_.category == CBehaviorCategory::HARVEST); }
        CWidget* GetTarget() const override { return target_; }
        void SetTarget(CWidget* t) override { target_ = t; targetHandle_ = t ? t->GetHandleId() : -1; }
        void SetTarget(float x, float y) override { tx_ = x; ty_ = y; }
        float GetRemaining() const override { return std::max(0.0f, spec_.duration - elapsed_); }
        void SetRemaining(float r) override { elapsed_ = std::max(0.0f, spec_.duration - r); }

        void Begin(CSimulation&) override {
            elapsed_ = 0.0f;
            tickAccum_ = 0.0f;
            beganAction_ = false;
            propelling_ = false;
            waypoints_.clear();
            pathRecalcCooldown_ = 0;
            movingToRange_ = (targetKind_ != COrder::TargetKind::NONE && spec_.range > 0.0f);
        }

        void End(CSimulation& s) override {
            propelling_ = false;
            waypoints_.clear();
            if (beganAction_ && spec_.onEnd && unit_) {
                spec_.onEnd(s, unit_);
            }
        }

        bool Update(CSimulation& s) override {
            if (!unit_ || unit_->IsDead()) return true;
            CWidget* resolvedTarget = (targetHandle_ >= 0) ? s.GetWidget(targetHandle_) : target_;
            if (targetKind_ == COrder::TargetKind::WIDGET && (!resolvedTarget || resolvedTarget->IsDead())) {
                propelling_ = false;
                return true;
            }
            const float aimX = (targetKind_ == COrder::TargetKind::WIDGET && resolvedTarget) ? resolvedTarget->GetX() : tx_;
            const float aimY = (targetKind_ == COrder::TargetKind::WIDGET && resolvedTarget) ? resolvedTarget->GetY() : ty_;

            if (movingToRange_) {
                const float dist = MoveIntoRange::Distance2D(unit_->GetX(), unit_->GetY(), aimX, aimY);
                if (dist <= spec_.range) {
                    movingToRange_ = false;
                    propelling_ = false;
                    unit_->TurnTowardPoint(aimX, aimY);
                } else {
                    if (unit_->IsRooted()) { propelling_ = false; return true; }
                    MoveIntoRange::RefreshWaypointsIfNeeded(s, unit_, aimX, aimY, waypoints_, pathRecalcCooldown_, true, true);
                    if (!waypoints_.empty()) {
                        MoveIntoRange::StepFrontWaypoint(s, unit_, waypoints_, pathRecalcCooldown_, propelling_, true);
                        return false;
                    }
                    MoveIntoRange::StepDirectValidated(s, unit_, aimX, aimY, dist - spec_.range + 8.0f, propelling_);
                    return false;
                }
            }

            propelling_ = false;
            if (!beganAction_) {
                beganAction_ = true;
                if (spec_.onBegin) {
                    spec_.onBegin(s, unit_, resolvedTarget, aimX, aimY);
                }
            }

            const float dt = s.GetSimStepTime();
            elapsed_ += dt;
            tickAccum_ += dt;
            const float interval = std::max(0.0f, spec_.tickInterval);
            if (spec_.onTick && (interval <= 0.0f || tickAccum_ >= interval)) {
                if (interval > 0.0f) tickAccum_ -= interval;
                if (spec_.onTick(s, unit_, resolvedTarget, aimX, aimY, elapsed_)) {
                    return true;
                }
            }

            if (spec_.duration > 0.0f && elapsed_ >= spec_.duration) {
                return true;
            }
            return (spec_.duration <= 0.0f && !spec_.onTick);
        }

    private:
        CustomBehaviorSpec spec_;
        COrder::TargetKind targetKind_ = COrder::TargetKind::NONE;
        CWidget* target_ = nullptr;
        int targetHandle_ = -1;
        float tx_ = 0.0f;
        float ty_ = 0.0f;
        float elapsed_ = 0.0f;
        float tickAccum_ = 0.0f;
        bool movingToRange_ = false;
        bool beganAction_ = false;
        bool propelling_ = false;
        std::deque<std::pair<float, float>> waypoints_;
        int pathRecalcCooldown_ = 0;
    };

    ++stats_.customBehaviorsDispatched;
    return new CBehaviorDataDriven(specIt->second, order);
}


// ============================================================================
// Phase 25: Vision/Fog Multipliers & Extra Sight Sources, Custom Passability,
//           and Module State Snapshot Serialization
// ============================================================================

void War3GameModule::SetPlayerSightMultiplier(int playerIndex, float multiplier) {
    if (playerIndex < 0) return;
    playerSightMultipliers_[playerIndex] = std::max(0.0f, multiplier);
}

float War3GameModule::GetPlayerSightMultiplier(int playerIndex) const {
    auto it = playerSightMultipliers_.find(playerIndex);
    return it != playerSightMultipliers_.end() ? it->second : 1.0f;
}

void War3GameModule::RegisterCustomSightSource(int sourceId, int playerIndex,
                                               float x, float y, float radius,
                                               bool isFlying) {
    if (sourceId == 0 || radius <= 0.0f) return;
    CFogSightSource src;
    src.player = playerIndex;
    src.playerMask = (playerIndex >= 0 && playerIndex < CFogSystem::kMaxPlayers && playerIndex < 32) ? (1u << playerIndex) : 0u;
    src.x = x;
    src.y = y;
    src.radius = radius;
    src.isFlying = isFlying;
    customSightSources_[sourceId] = src;
    ++stats_.customSightSourcesRegistered;
}

bool War3GameModule::UnregisterCustomSightSource(int sourceId) {
    return customSightSources_.erase(sourceId) > 0;
}

bool War3GameModule::HasCustomSightSource(int sourceId) const {
    return customSightSources_.count(sourceId) > 0;
}

float War3GameModule::OnComputeUnitSightRadius(const CSimulation& /*sim*/,
                                               const CUnit& unit,
                                               float baseSightRadius,
                                               bool /*isNight*/) const {
    ++stats_.sightRadiusEvaluations;
    const float mult = GetPlayerSightMultiplier(unit.GetPlayerIndex());
    return baseSightRadius * mult;
}

void War3GameModule::OnCollectExtraSightSources(const CSimulation& sim,
                                                std::vector<CFogSightSource>& outSources) const {
    for (const auto& [id, src] : customSightSources_) {
        (void)id;
        CFogSightSource expanded = src;
        if (const CPlayer* owner = sim.GetPlayer(src.player)) {
            for (int otherIdx = 0; otherIdx < CFogSystem::kMaxPlayers && otherIdx < 32; ++otherIdx) {
                if (otherIdx == src.player) continue;
                const CPlayer* otherPlayer = sim.GetPlayer(otherIdx);
                if (!otherPlayer) continue;
                if (owner->HasAlliance(*otherPlayer, CAllianceType::SHARED_VISION) ||
                    owner->HasAlliance(*otherPlayer, CAllianceType::VISION)) {
                    expanded.playerMask |= (1u << otherIdx);
                }
            }
        }
        outSources.push_back(expanded);
    }
}

void War3GameModule::SetCustomCellPassabilityRule(int cellX, int cellY, bool walkable, bool flyable) {
    const int64_t key = (static_cast<int64_t>(cellX) << 32) | static_cast<uint32_t>(cellY);
    customCellPassabilityRules_[key] = CustomCellPassabilityRule{walkable, flyable};
    ++stats_.customPassabilityRulesRegistered;
}

bool War3GameModule::RemoveCustomCellPassabilityRule(int cellX, int cellY) {
    const int64_t key = (static_cast<int64_t>(cellX) << 32) | static_cast<uint32_t>(cellY);
    return customCellPassabilityRules_.erase(key) > 0;
}

int War3GameModule::OnCheckCustomCellPassability(int cellX,
                                                 int cellY,
                                                 int movementTypeOrd) const {
    if (customCellPassabilityRules_.empty()) return -1;
    const int64_t key = (static_cast<int64_t>(cellX) << 32) | static_cast<uint32_t>(cellY);
    auto it = customCellPassabilityRules_.find(key);
    if (it == customCellPassabilityRules_.end()) return -1;
    const auto mt = static_cast<PathingGridMovementType>(movementTypeOrd);
    if (mt == PathingGridMovementType::DISABLED) return 1;
    if (mt == PathingGridMovementType::FLY) return it->second.flyable ? 1 : 0;
    return it->second.walkable ? 1 : 0;
}

namespace {

struct War3ModulePodSnapshot {
    uint32_t magic = 0x39324D57u; // "WM29"
    float defenseArmorFactor = 0.06f;
    float bountyMultiplier = 1.0f;
    float experienceMultiplier = 1.0f;
    int32_t winningPlayerIndex = -1;
    uint32_t mutedPlayerMask = 0;
    uint32_t tributeCommandsExecuted = 0;
    uint8_t flags = 0; // bit 0: meleeVictoryRulesEnabled, bit 1: requireZeroUnitsForDefeat, bit 2: matchEnded
    uint8_t reserved[3]{};
    uint32_t sightMultiplierCount = 0;
    uint32_t customSightSourceCount = 0;
    uint32_t customPassabilityCount = 0;
    uint32_t customCellCostCount = 0;
    uint32_t customFormationRankCount = 0;
    uint32_t customUpkeepLabelCount = 0;
    uint32_t customUnitSoundCount = 0;
    uint32_t customAssetPathAliasCount = 0;
};
static_assert(std::is_trivially_copyable_v<War3ModulePodSnapshot>,
              "War3ModulePodSnapshot must be trivially copyable");

struct War3SightMultiplierEntryPod {
    int32_t playerIndex = 0;
    float multiplier = 1.0f;
};
static_assert(std::is_trivially_copyable_v<War3SightMultiplierEntryPod>);

struct War3SightSourceEntryPod {
    int32_t sourceId = 0;
    int32_t player = -1;
    uint32_t playerMask = 0;
    float x = 0.0f;
    float y = 0.0f;
    float radius = 0.0f;
    uint8_t isFlying = 0;
    uint8_t reserved[3]{};
};
static_assert(std::is_trivially_copyable_v<War3SightSourceEntryPod>);

struct War3PassabilityRuleEntryPod {
    int64_t key = 0;
    uint8_t walkable = 1;
    uint8_t flyable = 1;
    uint8_t reserved[6]{};
};
static_assert(std::is_trivially_copyable_v<War3PassabilityRuleEntryPod>);

struct War3CellCostEntryPod {
    int64_t key = 0;
    float costMultiplier = 1.0f;
    uint32_t reserved = 0;
};
static_assert(std::is_trivially_copyable_v<War3CellCostEntryPod>);

struct War3FormationRankEntryPod {
    uint32_t unitTypeRaw = 0;
    int32_t rankPriority = 0;
};
static_assert(std::is_trivially_copyable_v<War3FormationRankEntryPod>);

template <typename T>
void AppendModulePod(std::vector<uint8_t>& buf, const T& val) {
    const uint8_t* p = reinterpret_cast<const uint8_t*>(&val);
    buf.insert(buf.end(), p, p + sizeof(T));
}

template <typename T>
bool ReadModulePod(const uint8_t* data, size_t size, size_t& offset, T& outVal) {
    if (offset + sizeof(T) > size) return false;
    std::memcpy(&outVal, data + offset, sizeof(T));
    offset += sizeof(T);
    return true;
}

void AppendModuleString(std::vector<uint8_t>& buf, const std::string& str) {
    const uint32_t len = static_cast<uint32_t>(str.size());
    AppendModulePod(buf, len);
    if (len > 0) {
        buf.insert(buf.end(), reinterpret_cast<const uint8_t*>(str.data()),
                   reinterpret_cast<const uint8_t*>(str.data()) + len);
    }
}

bool ReadModuleString(const uint8_t* data, size_t size, size_t& offset, std::string& outStr) {
    uint32_t len = 0;
    if (!ReadModulePod(data, size, offset, len)) return false;
    if (len > size - offset) return false;
    outStr.assign(reinterpret_cast<const char*>(data + offset), len);
    offset += len;
    return true;
}

} // namespace

void War3GameModule::OnCaptureModuleSnapshot(const CSimulation& /*sim*/,
                                             std::vector<uint8_t>& outBytes) const {
    ++stats_.moduleSnapshotsCaptured;
    War3ModulePodSnapshot pod;
    pod.defenseArmorFactor = defenseArmorFactor_;
    pod.bountyMultiplier = bountyMultiplier_;
    pod.experienceMultiplier = experienceMultiplier_;
    pod.winningPlayerIndex = winningPlayerIndex_;
    pod.mutedPlayerMask = mutedPlayerMask_;
    pod.tributeCommandsExecuted = tributeCommandsExecuted_;
    pod.flags = (meleeVictoryRulesEnabled_ ? 1u : 0u) |
                (requireZeroUnitsForDefeat_ ? 2u : 0u) |
                (matchEnded_ ? 4u : 0u);
    pod.sightMultiplierCount = static_cast<uint32_t>(playerSightMultipliers_.size());
    pod.customSightSourceCount = static_cast<uint32_t>(customSightSources_.size());
    pod.customPassabilityCount = static_cast<uint32_t>(customCellPassabilityRules_.size());
    pod.customCellCostCount = static_cast<uint32_t>(customCellCostMultipliers_.size());
    pod.customFormationRankCount = static_cast<uint32_t>(customUnitFormationRanks_.size());
    pod.customUpkeepLabelCount = static_cast<uint32_t>(customUpkeepLabels_.size());
    pod.customUnitSoundCount = static_cast<uint32_t>(customUnitSoundOverrides_.size());
    pod.customAssetPathAliasCount = static_cast<uint32_t>(customAssetPathAliases_.size());

    outBytes.clear();
    AppendModulePod(outBytes, pod);

    std::vector<War3SightMultiplierEntryPod> multEntries;
    multEntries.reserve(playerSightMultipliers_.size());
    for (const auto& [pIdx, mult] : playerSightMultipliers_) {
        multEntries.push_back(War3SightMultiplierEntryPod{pIdx, mult});
    }
    std::sort(multEntries.begin(), multEntries.end(),
              [](const auto& a, const auto& b) { return a.playerIndex < b.playerIndex; });
    for (const auto& e : multEntries) AppendModulePod(outBytes, e);

    std::vector<War3SightSourceEntryPod> srcEntries;
    srcEntries.reserve(customSightSources_.size());
    for (const auto& [sid, src] : customSightSources_) {
        War3SightSourceEntryPod e;
        e.sourceId = sid;
        e.player = src.player;
        e.playerMask = src.playerMask;
        e.x = src.x;
        e.y = src.y;
        e.radius = src.radius;
        e.isFlying = src.isFlying ? 1u : 0u;
        srcEntries.push_back(e);
    }
    std::sort(srcEntries.begin(), srcEntries.end(),
              [](const auto& a, const auto& b) { return a.sourceId < b.sourceId; });
    for (const auto& e : srcEntries) AppendModulePod(outBytes, e);

    std::vector<War3PassabilityRuleEntryPod> ruleEntries;
    ruleEntries.reserve(customCellPassabilityRules_.size());
    for (const auto& [k, rule] : customCellPassabilityRules_) {
        War3PassabilityRuleEntryPod e;
        e.key = k;
        e.walkable = rule.walkable ? 1u : 0u;
        e.flyable = rule.flyable ? 1u : 0u;
        ruleEntries.push_back(e);
    }
    std::sort(ruleEntries.begin(), ruleEntries.end(),
              [](const auto& a, const auto& b) { return a.key < b.key; });
    for (const auto& e : ruleEntries) AppendModulePod(outBytes, e);

    std::vector<War3CellCostEntryPod> costEntries;
    costEntries.reserve(customCellCostMultipliers_.size());
    for (const auto& [k, cost] : customCellCostMultipliers_) {
        costEntries.push_back(War3CellCostEntryPod{k, cost, 0u});
    }
    std::sort(costEntries.begin(), costEntries.end(),
              [](const auto& a, const auto& b) { return a.key < b.key; });
    for (const auto& e : costEntries) AppendModulePod(outBytes, e);

    std::vector<War3FormationRankEntryPod> rankEntries;
    rankEntries.reserve(customUnitFormationRanks_.size());
    for (const auto& [raw, rank] : customUnitFormationRanks_) {
        rankEntries.push_back(War3FormationRankEntryPod{raw, rank});
    }
    std::sort(rankEntries.begin(), rankEntries.end(),
              [](const auto& a, const auto& b) { return a.unitTypeRaw < b.unitTypeRaw; });
    for (const auto& e : rankEntries) AppendModulePod(outBytes, e);

    std::vector<int> upkeepKeys;
    upkeepKeys.reserve(customUpkeepLabels_.size());
    for (const auto& [lvl, _] : customUpkeepLabels_) upkeepKeys.push_back(lvl);
    std::sort(upkeepKeys.begin(), upkeepKeys.end());
    for (int lvl : upkeepKeys) {
        const int32_t levelPod = static_cast<int32_t>(lvl);
        AppendModulePod(outBytes, levelPod);
        AppendModuleString(outBytes, customUpkeepLabels_.at(lvl));
    }

    std::vector<uint32_t> soundKeys;
    soundKeys.reserve(customUnitSoundOverrides_.size());
    for (const auto& [raw, _] : customUnitSoundOverrides_) soundKeys.push_back(raw);
    std::sort(soundKeys.begin(), soundKeys.end());
    for (uint32_t raw : soundKeys) {
        AppendModulePod(outBytes, raw);
        const auto& entry = customUnitSoundOverrides_.at(raw);
        AppendModuleString(outBytes, entry.hitClip);
        AppendModuleString(outBytes, entry.deathClip);
    }

    std::vector<std::string> aliasKeys;
    aliasKeys.reserve(customAssetPathAliases_.size());
    for (const auto& [k, _] : customAssetPathAliases_) aliasKeys.push_back(k);
    std::sort(aliasKeys.begin(), aliasKeys.end());
    for (const std::string& k : aliasKeys) {
        AppendModuleString(outBytes, k);
        AppendModuleString(outBytes, customAssetPathAliases_.at(k));
    }
}

bool War3GameModule::OnRestoreModuleSnapshot(CSimulation& /*sim*/,
                                             const uint8_t* data,
                                             size_t size) {
    ++stats_.moduleSnapshotsRestored;
    if (!data || size < sizeof(War3ModulePodSnapshot)) return false;
    size_t offset = 0;
    War3ModulePodSnapshot pod{};
    if (!ReadModulePod(data, size, offset, pod)) return false;
    if (pod.magic != 0x39324D57u && pod.magic != 0x38324D57u &&
        pod.magic != 0x36324D57u && pod.magic != 0x35324D57u) return false;

    const size_t expectedExtra =
        static_cast<size_t>(pod.sightMultiplierCount) * sizeof(War3SightMultiplierEntryPod) +
        static_cast<size_t>(pod.customSightSourceCount) * sizeof(War3SightSourceEntryPod) +
        static_cast<size_t>(pod.customPassabilityCount) * sizeof(War3PassabilityRuleEntryPod) +
        static_cast<size_t>(pod.customCellCostCount) * sizeof(War3CellCostEntryPod) +
        static_cast<size_t>(pod.customFormationRankCount) * sizeof(War3FormationRankEntryPod);
    if (offset + expectedExtra > size) return false;

    defenseArmorFactor_ = pod.defenseArmorFactor;
    bountyMultiplier_ = pod.bountyMultiplier;
    experienceMultiplier_ = pod.experienceMultiplier;
    winningPlayerIndex_ = pod.winningPlayerIndex;
    mutedPlayerMask_ = pod.mutedPlayerMask;
    tributeCommandsExecuted_ = pod.tributeCommandsExecuted;
    meleeVictoryRulesEnabled_ = (pod.flags & 1u) != 0;
    requireZeroUnitsForDefeat_ = (pod.flags & 2u) != 0;
    matchEnded_ = (pod.flags & 4u) != 0;

    playerSightMultipliers_.clear();
    for (uint32_t i = 0; i < pod.sightMultiplierCount; ++i) {
        War3SightMultiplierEntryPod e{};
        if (!ReadModulePod(data, size, offset, e)) return false;
        playerSightMultipliers_[e.playerIndex] = e.multiplier;
    }

    customSightSources_.clear();
    for (uint32_t i = 0; i < pod.customSightSourceCount; ++i) {
        War3SightSourceEntryPod e{};
        if (!ReadModulePod(data, size, offset, e)) return false;
        CFogSightSource src;
        src.player = e.player;
        src.playerMask = e.playerMask;
        src.x = e.x;
        src.y = e.y;
        src.radius = e.radius;
        src.isFlying = (e.isFlying != 0);
        customSightSources_[e.sourceId] = src;
    }

    customCellPassabilityRules_.clear();
    for (uint32_t i = 0; i < pod.customPassabilityCount; ++i) {
        War3PassabilityRuleEntryPod e{};
        if (!ReadModulePod(data, size, offset, e)) return false;
        customCellPassabilityRules_[e.key] = CustomCellPassabilityRule{e.walkable != 0, e.flyable != 0};
    }

    customCellCostMultipliers_.clear();
    for (uint32_t i = 0; i < pod.customCellCostCount; ++i) {
        War3CellCostEntryPod e{};
        if (!ReadModulePod(data, size, offset, e)) return false;
        customCellCostMultipliers_[e.key] = e.costMultiplier;
    }

    customUnitFormationRanks_.clear();
    for (uint32_t i = 0; i < pod.customFormationRankCount; ++i) {
        War3FormationRankEntryPod e{};
        if (!ReadModulePod(data, size, offset, e)) return false;
        customUnitFormationRanks_[e.unitTypeRaw] = e.rankPriority;
    }

    customUpkeepLabels_.clear();
    for (uint32_t i = 0; i < pod.customUpkeepLabelCount; ++i) {
        int32_t lvl = 0;
        std::string label;
        if (!ReadModulePod(data, size, offset, lvl) ||
            !ReadModuleString(data, size, offset, label)) {
            return false;
        }
        customUpkeepLabels_[lvl] = std::move(label);
    }

    customUnitSoundOverrides_.clear();
    for (uint32_t i = 0; i < pod.customUnitSoundCount; ++i) {
        uint32_t raw = 0;
        CustomUnitSoundEntry entry;
        if (!ReadModulePod(data, size, offset, raw) ||
            !ReadModuleString(data, size, offset, entry.hitClip) ||
            !ReadModuleString(data, size, offset, entry.deathClip)) {
            return false;
        }
        customUnitSoundOverrides_[raw] = std::move(entry);
    }

    customAssetPathAliases_.clear();
    for (uint32_t i = 0; i < pod.customAssetPathAliasCount; ++i) {
        std::string key;
        std::string target;
        if (!ReadModuleString(data, size, offset, key) ||
            !ReadModuleString(data, size, offset, target)) {
            return false;
        }
        customAssetPathAliases_[std::move(key)] = std::move(target);
    }

    return true;
}

// ============================================================================
// Phase 26: Lockstep Command Validation, Global Allied Tribute & Custom
//           SimCommands, and Deterministic Module State Hashing
// ============================================================================

void War3GameModule::SetPlayerCommandMuted(int playerIndex, bool muted) {
    if (playerIndex < 0 || playerIndex >= 32) return;
    if (muted) {
        mutedPlayerMask_ |= (1u << playerIndex);
    } else {
        mutedPlayerMask_ &= ~(1u << playerIndex);
    }
}

bool War3GameModule::IsPlayerCommandMuted(int playerIndex) const {
    if (playerIndex < 0 || playerIndex >= 32) return false;
    return (mutedPlayerMask_ & (1u << playerIndex)) != 0u;
}

void War3GameModule::RegisterCustomGlobalCommand(int32_t orderId, CustomGlobalCommandFn fn) {
    if (orderId == 0 || !fn) return;
    customGlobalCommands_[orderId] = std::move(fn);
}

bool War3GameModule::UnregisterCustomGlobalCommand(int32_t orderId) {
    return customGlobalCommands_.erase(orderId) > 0;
}

bool War3GameModule::HasCustomGlobalCommand(int32_t orderId) const {
    return customGlobalCommands_.count(orderId) > 0;
}

bool War3GameModule::OnValidateLockstepCommand(const CSimulation& sim,
                                               const SimCommand& cmd) const {
    ++stats_.lockstepCommandsValidated;
    if (IsPlayerCommandMuted(cmd.player)) {
        return false;
    }
    if (const CPlayer* p = sim.GetPlayer(cmd.player)) {
        if (p->GetGameResult() == CPlayerGameResult::DEFEAT) {
            return false;
        }
    }
    return true;
}

bool War3GameModule::OnExecuteCustomSimCommand(CSimulation& sim,
                                               const SimCommand& cmd) {
    auto it = customGlobalCommands_.find(cmd.orderId);
    if (it != customGlobalCommands_.end() && it->second) {
        if (it->second(sim, cmd)) {
            ++stats_.customSimCommandsExecuted;
            return true;
        }
        return false;
    }

    // Built-in WarCraft 3 Allied Resource Tribute commands (TRGD / TRLM):
    // cmd.player = sender player, cmd.targetHandle = recipient player index, cmd.x = amount
    if (cmd.orderId == kOrderTributeGold || cmd.orderId == kOrderTributeLumber) {
        if (!std::isfinite(cmd.x) || cmd.x <= 0.0f || cmd.x > 100000000.0f) {
            return false;
        }
        CPlayer* sender = sim.GetPlayer(cmd.player);
        CPlayer* recipient = sim.GetPlayer(cmd.targetHandle);
        const int amount = static_cast<int>(cmd.x);
        if (!sender || !recipient || sender == recipient || amount <= 0) {
            return false;
        }
        if (cmd.orderId == kOrderTributeGold) {
            if (sender->GetGold() < amount) return false;
            sender->SetGold(sender->GetGold() - amount);
            recipient->SetGold(recipient->GetGold() + amount);
        } else {
            if (sender->GetLumber() < amount) return false;
            sender->SetLumber(sender->GetLumber() - amount);
            recipient->SetLumber(recipient->GetLumber() + amount);
        }
        ++tributeCommandsExecuted_;
        ++stats_.customSimCommandsExecuted;
        return true;
    }

    return false;
}

uint64_t War3GameModule::OnComputeModuleStateHash(const CSimulation& /*sim*/) const {
    ++stats_.moduleHashesComputed;
    uint64_t h = 1469598103934665603ull;
    auto mixU32 = [&h](uint32_t v) {
        for (int i = 0; i < 4; ++i) {
            h ^= static_cast<uint64_t>((v >> (i * 8)) & 0xFFu);
            h *= 1099511628211ull;
        }
    };
    auto mixFloat = [&mixU32](float v) {
        uint32_t bits = 0;
        std::memcpy(&bits, &v, sizeof(bits));
        mixU32(bits);
    };
    auto mixString = [&h, &mixU32](const std::string& s) {
        mixU32(static_cast<uint32_t>(s.size()));
        for (unsigned char c : s) {
            h ^= static_cast<uint64_t>(c);
            h *= 1099511628211ull;
        }
    };
    mixFloat(defenseArmorFactor_);
    mixFloat(bountyMultiplier_);
    mixFloat(experienceMultiplier_);
    mixU32(mutedPlayerMask_);
    mixU32(tributeCommandsExecuted_);
    mixU32(static_cast<uint32_t>(winningPlayerIndex_));
    const uint32_t flags = (meleeVictoryRulesEnabled_ ? 1u : 0u) |
                           (requireZeroUnitsForDefeat_ ? 2u : 0u) |
                           (matchEnded_ ? 4u : 0u);
    mixU32(flags);
    mixU32(static_cast<uint32_t>(customCellCostMultipliers_.size()));
    mixU32(static_cast<uint32_t>(customUnitFormationRanks_.size()));

    if (!playerSightMultipliers_.empty()) {
        std::vector<std::pair<int, float>> sortedMults(
            playerSightMultipliers_.begin(), playerSightMultipliers_.end());
        std::sort(sortedMults.begin(), sortedMults.end(),
                  [](const auto& a, const auto& b) { return a.first < b.first; });
        for (const auto& [pIdx, mult] : sortedMults) {
            mixU32(static_cast<uint32_t>(pIdx));
            mixFloat(mult);
        }
    }
    if (!customSightSources_.empty()) {
        std::vector<int> sortedIds;
        sortedIds.reserve(customSightSources_.size());
        for (const auto& [sid, _] : customSightSources_) sortedIds.push_back(sid);
        std::sort(sortedIds.begin(), sortedIds.end());
        for (int sid : sortedIds) {
            const auto& src = customSightSources_.at(sid);
            mixU32(static_cast<uint32_t>(sid));
            mixU32(static_cast<uint32_t>(src.player));
            mixU32(src.playerMask);
            mixFloat(src.x);
            mixFloat(src.y);
            mixFloat(src.radius);
            mixU32(src.isFlying ? 1u : 0u);
        }
    }
    if (!customCellPassabilityRules_.empty()) {
        std::vector<int64_t> sortedKeys;
        sortedKeys.reserve(customCellPassabilityRules_.size());
        for (const auto& [k, _] : customCellPassabilityRules_) sortedKeys.push_back(k);
        std::sort(sortedKeys.begin(), sortedKeys.end());
        for (int64_t k : sortedKeys) {
            const auto& r = customCellPassabilityRules_.at(k);
            mixU32(static_cast<uint32_t>(k & 0xFFFFFFFFull));
            mixU32(static_cast<uint32_t>((static_cast<uint64_t>(k) >> 32) & 0xFFFFFFFFull));
            mixU32((r.walkable ? 1u : 0u) | (r.flyable ? 2u : 0u));
        }
    }
    if (!customCellCostMultipliers_.empty()) {
        std::vector<int64_t> sortedKeys;
        sortedKeys.reserve(customCellCostMultipliers_.size());
        for (const auto& [k, _] : customCellCostMultipliers_) sortedKeys.push_back(k);
        std::sort(sortedKeys.begin(), sortedKeys.end());
        for (int64_t k : sortedKeys) {
            mixU32(static_cast<uint32_t>(k & 0xFFFFFFFFull));
            mixU32(static_cast<uint32_t>((static_cast<uint64_t>(k) >> 32) & 0xFFFFFFFFull));
            mixFloat(customCellCostMultipliers_.at(k));
        }
    }
    if (!customUnitFormationRanks_.empty()) {
        std::vector<uint32_t> sortedTypes;
        sortedTypes.reserve(customUnitFormationRanks_.size());
        for (const auto& [raw, _] : customUnitFormationRanks_) sortedTypes.push_back(raw);
        std::sort(sortedTypes.begin(), sortedTypes.end());
        for (uint32_t raw : sortedTypes) {
            mixU32(raw);
            mixU32(static_cast<uint32_t>(customUnitFormationRanks_.at(raw)));
        }
    }
    if (!customUpkeepLabels_.empty()) {
        std::vector<int> sortedLvls;
        sortedLvls.reserve(customUpkeepLabels_.size());
        for (const auto& [lvl, _] : customUpkeepLabels_) sortedLvls.push_back(lvl);
        std::sort(sortedLvls.begin(), sortedLvls.end());
        for (int lvl : sortedLvls) {
            mixU32(static_cast<uint32_t>(lvl));
            mixString(customUpkeepLabels_.at(lvl));
        }
    }
    if (!customUnitSoundOverrides_.empty()) {
        std::vector<uint32_t> sortedTypes;
        sortedTypes.reserve(customUnitSoundOverrides_.size());
        for (const auto& [raw, _] : customUnitSoundOverrides_) sortedTypes.push_back(raw);
        std::sort(sortedTypes.begin(), sortedTypes.end());
        for (uint32_t raw : sortedTypes) {
            mixU32(raw);
            const auto& entry = customUnitSoundOverrides_.at(raw);
            mixString(entry.hitClip);
            mixString(entry.deathClip);
        }
    }
    if (!customAssetPathAliases_.empty()) {
        std::vector<std::string> sortedKeys;
        sortedKeys.reserve(customAssetPathAliases_.size());
        for (const auto& [k, _] : customAssetPathAliases_) sortedKeys.push_back(k);
        std::sort(sortedKeys.begin(), sortedKeys.end());
        for (const std::string& k : sortedKeys) {
            mixString(k);
            mixString(customAssetPathAliases_.at(k));
        }
    }
    return h;
}

// ============================================================================
// Phase 27: Pluggable Cell Traversal Cost Multiplier & Squad Formation Rank
// ============================================================================

void War3GameModule::SetCustomCellCostMultiplier(int cellX, int cellY, float costMultiplier) {
    const int64_t key = (static_cast<int64_t>(cellX) << 32) | static_cast<uint32_t>(cellY);
    customCellCostMultipliers_[key] = std::max(0.1f, costMultiplier);
}

float War3GameModule::GetCustomCellCostMultiplier(int cellX, int cellY) const {
    const int64_t key = (static_cast<int64_t>(cellX) << 32) | static_cast<uint32_t>(cellY);
    auto it = customCellCostMultipliers_.find(key);
    return it != customCellCostMultipliers_.end() ? it->second : 1.0f;
}

bool War3GameModule::RemoveCustomCellCostMultiplier(int cellX, int cellY) {
    const int64_t key = (static_cast<int64_t>(cellX) << 32) | static_cast<uint32_t>(cellY);
    return customCellCostMultipliers_.erase(key) > 0;
}

float War3GameModule::OnGetCellTraversalCostMultiplier(int cellX,
                                                       int cellY,
                                                       int movementTypeOrd) const {
    if (customCellCostMultipliers_.empty()) return 1.0f;
    const auto mt = static_cast<PathingGridMovementType>(movementTypeOrd);
    if (mt == PathingGridMovementType::FLY || mt == PathingGridMovementType::DISABLED) {
        return 1.0f;
    }
    return GetCustomCellCostMultiplier(cellX, cellY);
}

void War3GameModule::SetCustomUnitFormationRank(War3ID unitTypeId, int rankPriority) {
    if (unitTypeId.value == 0) return;
    customUnitFormationRanks_[unitTypeId.value] = rankPriority;
}

int War3GameModule::GetCustomUnitFormationRank(War3ID unitTypeId) const {
    auto it = customUnitFormationRanks_.find(unitTypeId.value);
    return it != customUnitFormationRanks_.end() ? it->second : -1;
}

bool War3GameModule::RemoveCustomUnitFormationRank(War3ID unitTypeId) {
    return customUnitFormationRanks_.erase(unitTypeId.value) > 0;
}

int War3GameModule::OnGetUnitFormationRankPriority(const CUnit& unit) const {
    auto it = customUnitFormationRanks_.find(unit.GetTypeId().value);
    if (it != customUnitFormationRanks_.end()) {
        return it->second;
    }
    // Automatic WarCraft 3 tactical role classification:
    //   0 = Frontline melee / tank
    //   1 = Midline ranged combat
    //   2 = Backline long-range siege / non-combat caster
    if (const CUnitType* ut = unit.GetUnitType()) {
        const auto& attacks = ut->GetAttacks();
        if (attacks.empty()) return 2;
        const int range = attacks[0].range;
        if (range <= 160) return 0;
        if (range <= 550) return 1;
        return 2;
    }
    return 0;
}

// ============================================================================
// Phase 28: Pluggable Upkeep Display Label & Unit Combat/Death Sound Routing
// ============================================================================

void War3GameModule::SetCustomUpkeepLabel(int upkeepLevel, const std::string& label) {
    if (label.empty()) {
        customUpkeepLabels_.erase(upkeepLevel);
    } else {
        customUpkeepLabels_[upkeepLevel] = label;
    }
}

std::string War3GameModule::GetUpkeepDisplayLabel(int upkeepLevel) const {
    auto it = customUpkeepLabels_.find(upkeepLevel);
    if (it != customUpkeepLabels_.end()) {
        return it->second;
    }
    return (upkeepLevel == 2) ? "High Upkeep"
         : (upkeepLevel == 1) ? "Low Upkeep"
         : "No Upkeep";
}

void War3GameModule::RegisterCustomUnitSoundOverride(War3ID unitTypeId,
                                                     const std::string& hitClip,
                                                     const std::string& deathClip) {
    if (unitTypeId.value == 0) return;
    customUnitSoundOverrides_[unitTypeId.value] = CustomUnitSoundEntry{hitClip, deathClip};
}

bool War3GameModule::UnregisterCustomUnitSoundOverride(War3ID unitTypeId) {
    return customUnitSoundOverrides_.erase(unitTypeId.value) > 0;
}

bool War3GameModule::HasCustomUnitSoundOverride(War3ID unitTypeId) const {
    return customUnitSoundOverrides_.count(unitTypeId.value) > 0;
}

bool War3GameModule::OnResolveUnitCombatSound(const CSimulation& sim,
                                              int unitHandle,
                                              int /*sourceHandle*/,
                                              bool isDeath,
                                              std::string& outClipKey) const {
    const CUnit* u = sim.GetUnit(unitHandle);
    if (!u) return false;
    auto it = customUnitSoundOverrides_.find(u->GetTypeId().value);
    if (it != customUnitSoundOverrides_.end()) {
        const std::string& baseClip = isDeath ? it->second.deathClip : it->second.hitClip;
        if (!baseClip.empty()) {
            outClipKey = baseClip + "_" + std::to_string(unitHandle);
            return true;
        }
    }
    if (isDeath) {
        outClipKey = "Sound/Units/Death/" + u->GetTypeId().ToString() + "_" + std::to_string(unitHandle);
    } else {
        outClipKey = "Sound/Units/Combat/MetalMediumSlice_" + std::to_string(unitHandle);
    }
    return true;
}

// ============================================================================
// Phase 29: Pluggable Asset Path Aliases & VFS Redirection
// ============================================================================

void War3GameModule::RegisterAssetPathAlias(const std::string& fromUriOrPath,
                                            const std::string& toUriOrPath,
                                            resource::ResourceServer* targetServer) {
    const auto parsed = resource::ResourceServer::ParseUri(fromUriOrPath);
    if (parsed.canonicalCacheKey.empty() || toUriOrPath.empty()) return;
    customAssetPathAliases_[parsed.canonicalCacheKey] = toUriOrPath;
    if (targetServer) {
        targetServer->RegisterPathAlias(fromUriOrPath, toUriOrPath);
    }
}

bool War3GameModule::UnregisterAssetPathAlias(const std::string& fromUriOrPath,
                                              resource::ResourceServer* targetServer) {
    const auto parsed = resource::ResourceServer::ParseUri(fromUriOrPath);
    if (targetServer) {
        targetServer->UnregisterPathAlias(fromUriOrPath);
    }
    return customAssetPathAliases_.erase(parsed.canonicalCacheKey) > 0;
}

bool War3GameModule::HasAssetPathAlias(const std::string& fromUriOrPath) const {
    const auto parsed = resource::ResourceServer::ParseUri(fromUriOrPath);
    return customAssetPathAliases_.count(parsed.canonicalCacheKey) > 0;
}

bool War3GameModule::OnResolveAssetPathAlias(const std::string& uriOrPath,
                                             std::string& outRedirectedUriOrPath) const {
    const auto parsed = resource::ResourceServer::ParseUri(uriOrPath);
    auto it = customAssetPathAliases_.find(parsed.canonicalCacheKey);
    if (it == customAssetPathAliases_.end()) {
        return false;
    }
    outRedirectedUriOrPath = it->second;
    return true;
}

} // namespace war3
