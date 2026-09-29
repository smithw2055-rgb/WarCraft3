#include "ability_factory.h"
#include "ecs/ability.h"
#include "ecs/abilities/ability_attack.h"
#include "ecs/abilities/ability_move.h"
#include "ecs/abilities/ability_hero.h"
#include "ecs/abilities/ability_inventory.h"
#include "ecs/abilities/ability_build.h"
#include "ecs/abilities/ability_harvest.h"
#include "ecs/abilities/ability_gold_mine.h"
#include "ecs/abilities/ability_return_resources.h"
#include "ecs/abilities/ability_queue.h"
#include "ecs/abilities/ability_rally.h"
#include "ecs/abilities/ability_upgrade.h"
#include "ecs/abilities/ability_sell_items.h"
#include "ecs/abilities/ability_repair.h"
#include "ecs/abilities/ability_transport.h"
#include "ecs/abilities/ability_root.h"
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
#include "ecs/abilities/auras.h"
#include "ecs/unit.h"
#include "ecs/csimulation.h"
#include <iostream>

std::unique_ptr<CAbility> AbilityFactory::CreateAbility(int handleId, War3ID code) {
    // Core abilities
    if (code == War3ID("Aatk")) return std::make_unique<CAbilityAttack>(handleId);
    if (code == War3ID("Amov")) return std::make_unique<CAbilityMove>(handleId);

    // Hero
    if (code == War3ID("AHer")) return std::make_unique<CAbilityHero>(handleId);

    // Inventory
    if (code == War3ID("AInv")) return std::make_unique<CAbilityInventory>(handleId);

    // Build abilities
    if (code == War3ID("AHbu")) return std::make_unique<CAbilityBuild>(handleId, code, CBuildRace::HUMAN);
    if (code == War3ID("AObu")) return std::make_unique<CAbilityBuild>(handleId, code, CBuildRace::ORC);
    if (code == War3ID("AUbu")) return std::make_unique<CAbilityBuild>(handleId, code, CBuildRace::UNDEAD);
    if (code == War3ID("AEbu")) return std::make_unique<CAbilityBuild>(handleId, code, CBuildRace::NIGHT_ELF);

    // Harvest / Return / Repair
    if (code == War3ID("Ahar")) return std::make_unique<CAbilityHarvest>(handleId);
    if (code == War3ID("Aret")) return std::make_unique<CAbilityReturnResources>(handleId);
    if (code == War3ID("Arep") || code == War3ID("Ahrp")
        || code == War3ID("Aren") || code == War3ID("Arst")) {
        auto ability = std::make_unique<CAbilityRepair>(handleId);
        ability->SetAlias(code);
        return ability;
    }

    // Gold mine
    if (code == War3ID("Agld")) return std::make_unique<CAbilityGoldMine>(handleId);

    // Queue / Rally / Upgrade
    if (code == War3ID("Aque")) return std::make_unique<CAbilityQueue>(handleId);
    if (code == War3ID("ARal")) return std::make_unique<CAbilityRally>(handleId);
    if (code == War3ID("Aupg")) return std::make_unique<CAbilityUpgrade>(handleId);

    // Transport
    if (code == War3ID("Aloa")) return std::make_unique<CAbilityLoad>(handleId);
    if (code == War3ID("Adro")) return std::make_unique<CAbilityUnload>(handleId);

    // Root/Uproot
    if (code == War3ID("Aroo")) return std::make_unique<CAbilityRoot>(handleId);
    if (code == War3ID("Aro1")) return std::make_unique<CAbilityUproot>(handleId);

    // Shop
    if (code == War3ID("Asei")) return std::make_unique<CAbilitySellItems>(handleId);

    // ===== Human Spells =====
    if (code == War3ID("AHbz")) return std::make_unique<CAbilityBlizzard>(handleId);
    if (code == War3ID("AHhb")) return std::make_unique<CAbilityHolyLight>(handleId);
    if (code == War3ID("AHds")) return std::make_unique<CAbilityDivineShield>(handleId);
    if (code == War3ID("AHtc")) return std::make_unique<CAbilityThunderClap>(handleId);
    if (code == War3ID("AHtb")) return std::make_unique<CAbilityStormBolt>(handleId);

    // ===== Orc Spells =====
    if (code == War3ID("AOws")) return std::make_unique<CAbilityWarStomp>(handleId);
    if (code == War3ID("AOcl")) return std::make_unique<CAbilityChainLightning>(handleId);
    if (code == War3ID("AOhw")) return std::make_unique<CAbilityHealingWave>(handleId);
    if (code == War3ID("AOhx")) return std::make_unique<CAbilityHex>(handleId);
    if (code == War3ID("AOsf")) return std::make_unique<CAbilityFeralSpirit>(handleId);
    if (code == War3ID("AOeq")) return std::make_unique<CAbilityEarthquake>(handleId);

    // ===== Undead Spells =====
    if (code == War3ID("AUdc")) return std::make_unique<CAbilityDeathCoil>(handleId);
    if (code == War3ID("AUfn")) return std::make_unique<CAbilityFrostNova>(handleId);
    if (code == War3ID("AUcs")) return std::make_unique<CAbilityCarrionSwarm>(handleId);
    if (code == War3ID("AUsl")) return std::make_unique<CAbilitySleep>(handleId);
    if (code == War3ID("AUin")) return std::make_unique<CAbilityInferno>(handleId);

    // ===== Night Elf Spells =====
    if (code == War3ID("AEmb")) return std::make_unique<CAbilityManaBurn>(handleId);
    if (code == War3ID("AEer")) return std::make_unique<CAbilityEntanglingRoots>(handleId);

    // ===== Neutral Spells =====
    if (code == War3ID("ANsi")) return std::make_unique<CAbilitySilence>(handleId);
    if (code == War3ID("ANba")) return std::make_unique<CAbilityBlackArrow>(handleId);
    if (code == War3ID("ANdr")) return std::make_unique<CAbilityLifeDrain>(handleId);
    if (code == War3ID("ANfd")) return std::make_unique<CAbilityFingerOfDeath>(handleId);
    if (code == War3ID("ANdp")) return std::make_unique<CAbilityDarkPortal>(handleId);
    if (code == War3ID("ANrc")) return std::make_unique<CAbilityRainOfChaos>(handleId);

    // ===== Batch 3 hero spells =====
    if (code == War3ID("AHmt")) return std::make_unique<CAbilityMassTeleport>(handleId);
    if (code == War3ID("AHwe")) return std::make_unique<CAbilitySummonWaterElemental>(handleId);
    if (code == War3ID("AHre")) return std::make_unique<CAbilityResurrect>(handleId);
    if (code == War3ID("AEbl")) return std::make_unique<CAbilityBlink>(handleId);
    if (code == War3ID("AOmi")) return std::make_unique<CAbilityMirrorImage>(handleId);
    if (code == War3ID("AOsh")) return std::make_unique<CAbilityShockwave>(handleId);
    if (code == War3ID("AEst")) return std::make_unique<CAbilityOwlScout>(handleId);
    if (code == War3ID("AEsf")) return std::make_unique<CAbilityStarfall>(handleId);
    if (code == War3ID("AEtq")) return std::make_unique<CAbilityTranquility>(handleId);
    if (code == War3ID("AOre")) return std::make_unique<CAbilityReincarnation>(handleId);
    if (code == War3ID("ANch")) return std::make_unique<CAbilityCharm>(handleId);

    // ===== Batch 4 hero spells =====
    if (code == War3ID("AEev")) return std::make_unique<CAbilityEvasion>(handleId);
    if (code == War3ID("AOcr")) return std::make_unique<CAbilityCriticalStrike>(handleId);
    if (code == War3ID("AHbh")) return std::make_unique<CAbilityBash>(handleId);
    if (code == War3ID("Aslo")) return std::make_unique<CAbilitySlow>(handleId);
    if (code == War3ID("AUfa")) return std::make_unique<CAbilityFrostArmor>(handleId);
    if (code == War3ID("Afae")) return std::make_unique<CAbilityFaerieFire>(handleId);
    if (code == War3ID("AUdp")) return std::make_unique<CAbilityDeathPact>(handleId);
    if (code == War3ID("AUan")) return std::make_unique<CAbilityAnimateDead>(handleId);
    if (code == War3ID("ANdo")) return std::make_unique<CAbilityDoom>(handleId);
    if (code == War3ID("ANvc")) return std::make_unique<CAbilityVolcano>(handleId);

    // ===== Batch 5 hero spells =====
    if (code == War3ID("AHfs")) return std::make_unique<CAbilityFlameStrike>(handleId);
    if (code == War3ID("AUdd")) return std::make_unique<CAbilityDeathAndDecay>(handleId);
    if (code == War3ID("AUim")) return std::make_unique<CAbilityImpale>(handleId);
    if (code == War3ID("AEfk")) return std::make_unique<CAbilityFanOfKnives>(handleId);
    if (code == War3ID("ANrf")) return std::make_unique<CAbilityRainOfFire>(handleId);
    if (code == War3ID("AUts")) return std::make_unique<CAbilitySpikedCarapace>(handleId);
    if (code == War3ID("AUdr")) return std::make_unique<CAbilityDarkRitual>(handleId);
    if (code == War3ID("AEsh")) return std::make_unique<CAbilityShadowStrike>(handleId);

    // ===== Batch 6: alchemist kit + unit abilities =====
    if (code == War3ID("ANhs")) return std::make_unique<CAbilityHealingSpray>(handleId);
    if (code == War3ID("ANab")) return std::make_unique<CAbilityAcidBomb>(handleId);
    if (code == War3ID("ANcr")) return std::make_unique<CAbilityChemicalRage>(handleId);
    if (code == War3ID("ANtm")) return std::make_unique<CAbilityTransmute>(handleId);
    if (code == War3ID("Ablo")) return std::make_unique<CAbilityBloodlustCast>(handleId);
    if (code == War3ID("Aprg")) return std::make_unique<CAbilityPurge>(handleId);
    if (code == War3ID("Alsh")) return std::make_unique<CAbilityLightningShield>(handleId);
    if (code == War3ID("Arai")) return std::make_unique<CAbilityRaiseDead>(handleId);

    // ===== Batch 7: unit abilities =====
    if (code == War3ID("AOsw")) return std::make_unique<CAbilitySerpentWard>(handleId);
    if (code == War3ID("Aply")) return std::make_unique<CAbilityPolymorph>(handleId);
    if (code == War3ID("Ainf")) return std::make_unique<CAbilityInnerFire>(handleId);
    if (code == War3ID("Ahea")) return std::make_unique<CAbilityHeal>(handleId);
    if (code == War3ID("Adis")) return std::make_unique<CAbilityDispelMagic>(handleId);
    if (code == War3ID("Arej")) return std::make_unique<CAbilityRejuvenation>(handleId);
    if (code == War3ID("Aroa")) return std::make_unique<CAbilityRoar>(handleId);
    if (code == War3ID("Adef")) return std::make_unique<CAbilityDefend>(handleId);

    // ===== Batch 8: charged-item abilities =====
    if (code == War3ID("AIha")) return std::make_unique<CAbilityItemHealGroup>(handleId);
    if (code == War3ID("AItp")) return std::make_unique<CAbilityItemTownPortal>(handleId);
    if (code == War3ID("AIvl")) return std::make_unique<CAbilityItemInvulnerable>(handleId);
    if (code == War3ID("AIh1")) return std::make_unique<CAbilityItemHealSelf>(handleId);
    if (code == War3ID("AIm1")) return std::make_unique<CAbilityItemManaSelf>(handleId);
    if (code == War3ID("AIpl")) return std::make_unique<CAbilityItemClarity>(handleId);
    if (code == War3ID("Aste")) return std::make_unique<CAbilityItemManaSteal>(handleId);
    if (code == War3ID("AIda")) return std::make_unique<CAbilityItemProtection>(handleId);

    // ===== Batch 9: orb modifiers + wand/scroll reuse =====
    if (code == War3ID("AIob")) return std::make_unique<CAbilityOrbOfFrost>(handleId);
    if (code == War3ID("AIcb")) return std::make_unique<CAbilityOrbOfCorruption>(handleId);
    if (code == War3ID("AIfb")) return std::make_unique<CAbilityOrbOfFire>(handleId);
    if (code == War3ID("AIil")) return std::make_unique<CAbilityIllusion>(handleId);
    if (code == War3ID("AIls")) return std::make_unique<CAbilityLightningShieldWand>(handleId);
    if (code == War3ID("AIrc")) return std::make_unique<CAbilityReincarnation>(handleId, code, 0);
    if (code == War3ID("AIrr")) return std::make_unique<CAbilityBeastScroll>(handleId);
    if (code == War3ID("AIhw")) return std::make_unique<CAbilityHealingWard>(handleId);

    // ===== Batch 10: attack passives + misc unit abilities =====
    if (code == War3ID("ANca")) return std::make_unique<CAbilityCleavingAttack>(handleId);
    if (code == War3ID("AHfa")) return std::make_unique<CAbilitySearingArrows>(handleId);
    if (code == War3ID("Aens")) return std::make_unique<CAbilityEnsnare>(handleId);
    if (code == War3ID("Aast")) return std::make_unique<CAbilityAncestralSpirit>(handleId);
    if (code == War3ID("Ahwd")) return std::make_unique<CAbilityHealingWardUnit>(handleId);
    if (code == War3ID("Auco")) return std::make_unique<CAbilityUnstableConcoction>(handleId);
    if (code == War3ID("Aadm")) return std::make_unique<CAbilityAbolishMagic>(handleId);
    if (code == War3ID("ANcs")) return std::make_unique<CAbilityClusterRockets>(handleId);

    // ===== Batch 11: stealth + misc =====
    if (code == War3ID("AOwk")) return std::make_unique<CAbilityWindWalk>(handleId);
    if (code == War3ID("Aivs")) return std::make_unique<CAbilityInvisibility>(handleId);
    if (code == War3ID("Apiv")) return std::make_unique<CAbilityPermanentInvisibility>(handleId);
    if (code == War3ID("Ashm")) return std::make_unique<CAbilityShadowmeld>(handleId);
    if (code == War3ID("AItb")) return std::make_unique<CAbilityDustOfAppearance>(handleId);
    if (code == War3ID("Absk")) return std::make_unique<CAbilityBerserk>(handleId);
    if (code == War3ID("ANsg")) return std::make_unique<CAbilitySummonBear>(handleId);
    if (code == War3ID("ANbr")) return std::make_unique<CAbilityBattleRoar>(handleId);

    // ===== Batch 12: detection + misc =====
    if (code == War3ID("Atru")) return std::make_unique<CAbilityTrueSight>(handleId);
    if (code == War3ID("Adet")) {
        auto ability = std::make_unique<CAbilityTrueSight>(handleId);
        ability->SetAlias(code);
        return ability;
    }
    if (code == War3ID("Aeye")) return std::make_unique<CAbilitySentryWard>(handleId);
    if (code == War3ID("AIsw")) {
        auto ability = std::make_unique<CAbilitySentryWard>(handleId);
        ability->SetAlias(code);
        return ability;
    }
    if (code == War3ID("Afla")) return std::make_unique<CAbilityFlare>(handleId);
    if (code == War3ID("Abur")) return std::make_unique<CAbilityBurrow>(handleId);
    if (code == War3ID("Acan")) return std::make_unique<CAbilityCannibalize>(handleId);
    if (code == War3ID("AUcb")) return std::make_unique<CAbilityCarrionBeetles>(handleId);
    if (code == War3ID("Aweb")) return std::make_unique<CAbilityWeb>(handleId);

    // ===== Batch 13: morphs + misc =====
    if (code == War3ID("AEme")) return std::make_unique<CAbilityMetamorphosis>(handleId);
    if (code == War3ID("Abrf")) return std::make_unique<CAbilityBearForm>(handleId);
    if (code == War3ID("Arav")) return std::make_unique<CAbilityRavenForm>(handleId);
    if (code == War3ID("ANrg")) return std::make_unique<CAbilityRoboGoblin>(handleId);
    if (code == War3ID("Astn")) return std::make_unique<CAbilityStoneForm>(handleId);
    if (code == War3ID("AHav")) return std::make_unique<CAbilityAvatar>(handleId);
    if (code == War3ID("AHdr")) return std::make_unique<CAbilitySiphonMana>(handleId);
    if (code == War3ID("AOvd")) return std::make_unique<CAbilityBigBadVoodoo>(handleId);

    // ===== Batch 14: spawners + misc =====
    if (code == War3ID("ANsy")) return std::make_unique<CAbilityPocketFactory>(handleId);
    if (code == War3ID("ANlm")) return std::make_unique<CAbilityLavaSpawn>(handleId);
    if (code == War3ID("ANsb")) return std::make_unique<CAbilitySoulBurn>(handleId);
    if (code == War3ID("ANsq")) return std::make_unique<CAbilityQuilbeast>(handleId);
    if (code == War3ID("Afsh")) return std::make_unique<CAbilityFrostBreath>(handleId);
    if (code == War3ID("Aapl")) return std::make_unique<CAbilityDiseaseCloud>(handleId);
    if (code == War3ID("Awar")) return std::make_unique<CAbilityPulverize>(handleId);
    if (code == War3ID("ANfl")) return std::make_unique<CAbilityForkedLightning>(handleId);

    // ===== Batch 15 (abilities): phoenix =====
    if (code == War3ID("AHpx")) return std::make_unique<CAbilityPhoenix>(handleId);

    // ===== Batch 16 (abilities): devour + misc =====
    if (code == War3ID("Adev")) return std::make_unique<CAbilityDevour>(handleId);
    if (code == War3ID("Advm")) return std::make_unique<CAbilityDevourMagic>(handleId);
    if (code == War3ID("AHca")) return std::make_unique<CAbilityColdArrows>(handleId);
    if (code == War3ID("ANfa")) {
        auto ability = std::make_unique<CAbilityColdArrows>(handleId);
        ability->SetAlias(code);
        return ability;
    }
    if (code == War3ID("ANmo")) return std::make_unique<CAbilityMonsoon>(handleId);
    if (code == War3ID("Aabs")) return std::make_unique<CAbilityAbsorbMana>(handleId);
    if (code == War3ID("Auhf")) return std::make_unique<CAbilityUnholyFrenzy>(handleId);

    // ===== Batch 17 (abilities): howl, brews, shield, swarm, cyclone, vengeance =====
    if (code == War3ID("ANht")) return std::make_unique<CAbilityHowlOfTerror>(handleId);
    if (code == War3ID("ANbf")) return std::make_unique<CAbilityBreathOfFire>(handleId);
    if (code == War3ID("ANdh")) return std::make_unique<CAbilityDrunkenHaze>(handleId);
    if (code == War3ID("ANms")) return std::make_unique<CAbilityManaShield>(handleId);
    if (code == War3ID("AUls")) return std::make_unique<CAbilityLocustSwarm>(handleId);
    if (code == War3ID("Acyc")) return std::make_unique<CAbilityCyclone>(handleId);
    if (code == War3ID("Avng")) return std::make_unique<CAbilityVengeance>(handleId);

    // ===== Batch 18 (abilities): ref-skills gap closure =====
    if (code == War3ID("AOww")) return std::make_unique<CAbilityWhirlwind>(handleId);
    if (code == War3ID("AEim")) return std::make_unique<CAbilityImmolation>(handleId);
    if (code == War3ID("AEfn")) return std::make_unique<CAbilityForceOfNature>(handleId);
    if (code == War3ID("ANsw")) return std::make_unique<CAbilitySummonHawk>(handleId);
    if (code == War3ID("ANfy")) return std::make_unique<CAbilityFactory>(handleId);
    if (code == War3ID("Asds")) return std::make_unique<CAbilityKaboom>(handleId);
    if (code == War3ID("Apxf")) return std::make_unique<CAbilityPhoenixFire>(handleId);

    // ===== Batch 19: banish line =====
    if (code == War3ID("AHbn")) return std::make_unique<CAbilityBanish>(handleId);
    if (code == War3ID("Aam2")) return std::make_unique<CAbilityAntiMagicShell>(handleId);

    // ===== Batch 20: possession + stampede =====
    if (code == War3ID("Apos")) return std::make_unique<CAbilityPossession>(handleId);
    if (code == War3ID("ANst")) return std::make_unique<CAbilityStampede>(handleId);

    // ===== Batch 21: stasis trap + tornado =====
    if (code == War3ID("Asta")) return std::make_unique<CAbilityStasisTrap>(handleId);
    if (code == War3ID("ANto")) return std::make_unique<CAbilityTornado>(handleId);

    // ===== Batch 22: alias-only retail codes (owned by spells22) =====
    if (auto alias22 = CreateSpells22Alias(handleId, code)) return alias22;

    // ===== Batch 23: chain dispel + taunt =====
    if (code == War3ID("Ache")) return std::make_unique<CAbilityChainDispel>(handleId);
    if (code == War3ID("ANta")) return std::make_unique<CAbilityTaunt>(handleId);

    // ===== Batch 24: ref templates that were only aliases or item passives =====
    if (code == War3ID("Aoar")) return std::make_unique<CAbilityRegenerationAura>(handleId);
    if (code == War3ID("Aabr")) {
        auto ability = std::make_unique<CAbilityRegenerationAura>(handleId);
        ability->SetAlias(code);
        return ability;
    }
    if (code == War3ID("Aarm")) return std::make_unique<CAbilityManaRegenerationAura>(handleId);
    if (code == War3ID("AIas")) return std::make_unique<CAbilityItemAttackSpeed>(handleId);
    if (code == War3ID("AIrm")) return std::make_unique<CAbilityItemManaRegen>(handleId);
    if (code == War3ID("AIms")) return std::make_unique<CAbilityItemMoveSpeed>(handleId);
    if (code == War3ID("AIva")) return std::make_unique<CAbilityItemVampirism>(handleId);

    // Same ability-builder template as an ability already implemented.
    if (code == War3ID("Aasl")) {
        auto ability = std::make_unique<CAbilityEnduranceAura>(handleId);
        ability->SetAlias(code);
        return ability;
    }
    if (code == War3ID("Aakb")) {
        auto ability = std::make_unique<CAbilityCommandAura>(handleId);
        ability->SetAlias(code);
        return ability;
    }
    if (code == War3ID("ACrn")) {
        return std::make_unique<CAbilityReincarnation>(handleId, code);
    }
    if (code == War3ID("ANdb")) {
        // Drunken Brawler: the crit half plus the dodge half.
        return std::make_unique<CAbilityDrunkenBrawler>(handleId);
    }
    if (code == War3ID("AIrg")) {
        auto ability = std::make_unique<CAbilityRejuvenation>(handleId);
        ability->SetAlias(code);
        return ability;
    }
    if (code == War3ID("AIrd")) {
        auto ability = std::make_unique<CAbilityRaiseDead>(handleId);
        ability->SetAlias(code);
        return ability;
    }
    if (code == War3ID("ANfb")) {
        auto ability = std::make_unique<CAbilityStormBolt>(handleId);
        ability->SetAlias(code);
        return ability;
    }
    if (code == War3ID("Artn")) {
        auto ability = std::make_unique<CAbilityReturnResources>(handleId);
        ability->SetAlias(code);
        return ability;
    }
    if (code == War3ID("Agyv") || code == War3ID("Adts") || code == War3ID("Abdt")) {
        auto ability = std::make_unique<CAbilityTrueSight>(handleId);
        ability->SetAlias(code);
        return ability;
    }

    // ===== Batch 25: curse, moon well, eat tree, sacrifice, items =====
    if (code == War3ID("Acrs")) return std::make_unique<CAbilityCurse>(handleId);
    if (code == War3ID("Ambt")) return std::make_unique<CAbilityMoonWell>(handleId);
    if (code == War3ID("Aeat")) return std::make_unique<CAbilityEatTree>(handleId);
    if (code == War3ID("Asac")) return std::make_unique<CAbilitySacrifice>(handleId);
    if (code == War3ID("Alam")) return std::make_unique<CAbilityRequestSacrifice>(handleId);
    if (code == War3ID("Ahid")) return std::make_unique<CAbilityHide>(handleId);
    if (code == War3ID("Aeth")) return std::make_unique<CAbilityEthereal>(handleId);
    if (code == War3ID("AIgo")) return std::make_unique<CAbilityChestOfGold>(handleId);
    if (code == War3ID("AIat")) return std::make_unique<CAbilityItemDamageBonus>(handleId);
    if (code == War3ID("AIde")) return std::make_unique<CAbilityItemArmorBonus>(handleId);
    if (code == War3ID("AIml")) return std::make_unique<CAbilityItemLifeBonus>(handleId);
    if (code == War3ID("AImm")) return std::make_unique<CAbilityItemManaPoolBonus>(handleId);
    if (code == War3ID("AIab")) return std::make_unique<CAbilityItemAttributeBonus>(handleId);

    // ===== Batch 26: shield, tomes, forms, clock, invulnerability =====
    if (code == War3ID("ANss")) return std::make_unique<CAbilitySpellShieldPassive>(handleId);
    if (code == War3ID("ANse")) return std::make_unique<CAbilitySpellShieldArea>(handleId);
    if (code == War3ID("AIim")) return std::make_unique<CAbilityItemPermanentStats>(handleId, code, 0, 0, 1);
    if (code == War3ID("AIam")) return std::make_unique<CAbilityItemPermanentStats>(handleId, code, 1, 0, 0);
    if (code == War3ID("AIxm")) return std::make_unique<CAbilityItemPermanentStats>(handleId, code, 0, 1, 0);
    if (code == War3ID("AIsm")) return std::make_unique<CAbilityItemPermanentStats>(handleId, code, 1, 1, 1);
    if (code == War3ID("AImi")) return std::make_unique<CAbilityItemPermanentLife>(handleId);
    if (code == War3ID("AIem")) return std::make_unique<CAbilityItemExperience>(handleId);
    if (code == War3ID("AIlm")) return std::make_unique<CAbilityItemLevelGain>(handleId);
    if (code == War3ID("AIfs")) return std::make_unique<CAbilityItemFigurine>(handleId);
    if (code == War3ID("Avul")) return std::make_unique<CAbilityPermanentInvulnerable>(handleId);
    if (code == War3ID("AIct")) return std::make_unique<CAbilityChangeTimeOfDay>(handleId);
    if (code == War3ID("Acpf") || code == War3ID("Aetf") || code == War3ID("Aave")) {
        return std::make_unique<CAbilityToggleForm>(handleId, code);
    }
    if (code == War3ID("Acha")) return std::make_unique<CAbilityChaos>(handleId);

    // ===== Batch 27: state passives, leash, replenish, item reactions =====
    if (code == War3ID("Aetl")) {
        return std::make_unique<CAbilityStatePassive>(handleId, code, CPassiveState::Ethereal);
    }
    if (code == War3ID("Amim")) {
        return std::make_unique<CAbilityStatePassive>(handleId, code, CPassiveState::MagicImmune);
    }
    if (code == War3ID("Arsk")) {
        return std::make_unique<CAbilityStatePassive>(handleId, code, CPassiveState::Resistant);
    }
    if (code == War3ID("Agho")) {
        return std::make_unique<CAbilityStatePassive>(handleId, code, CPassiveState::Ghost);
    }
    if (code == War3ID("AIsr")) return std::make_unique<CAbilityItemSpellResist>(handleId);
    if (code == War3ID("AIdd")) return std::make_unique<CAbilityItemMixedDefense>(handleId);
    if (code == War3ID("Amdf")) return std::make_unique<CAbilityMagicDefense>(handleId);
    if (code == War3ID("Amls")) return std::make_unique<CAbilityMagicLeash>(handleId);
    if (code == War3ID("Arpl")) return std::make_unique<CAbilityReplenish>(handleId, code, true);
    if (code == War3ID("Arpm")) return std::make_unique<CAbilityReplenish>(handleId, code, false);
    if (code == War3ID("Asph")) return std::make_unique<CAbilitySphere>(handleId);
    if (code == War3ID("AIco")) {
        auto ability = std::make_unique<CAbilityCharm>(handleId);
        ability->SetAlias(code);
        return ability;
    }

    // ===== Batch 28: pairing, blight, harvest, cargo, waygate, shops =====
    if (code == War3ID("Amic")) {
        return std::make_unique<CAbilityPairing>(handleId, code, CPairMode::MilitiaCall);
    }
    if (code == War3ID("Amil")) {
        return std::make_unique<CAbilityPairing>(handleId, code, CPairMode::Militia);
    }
    if (code == War3ID("Acoa")) {
        return std::make_unique<CAbilityPairing>(handleId, code, CPairMode::Mount);
    }
    if (code == War3ID("Acoh")) {
        return std::make_unique<CAbilityPairing>(handleId, code, CPairMode::LoadPartner);
    }
    if (code == War3ID("Acoi")) {
        return std::make_unique<CAbilityPairing>(handleId, code, CPairMode::Couple);
    }
    if (code == War3ID("Abli")) return std::make_unique<CAbilityBlight>(handleId);
    if (code == War3ID("Abgm")) return std::make_unique<CAbilityBlightedGoldMine>(handleId);
    if (code == War3ID("Agl2")) return std::make_unique<CAbilityOverlayedGoldMine>(handleId);
    if (code == War3ID("Aaha")) return std::make_unique<CAbilityAcolyteHarvest>(handleId);
    if (code == War3ID("Awha")) return std::make_unique<CAbilityWispHarvest>(handleId);
    if (code == War3ID("Ahrl")) return std::make_unique<CAbilityHarvestLumber>(handleId);
    if (code == War3ID("Acar")) return std::make_unique<CAbilityCargoHold>(handleId, code, false);
    if (code == War3ID("Abun")) return std::make_unique<CAbilityCargoHold>(handleId, code, true);
    if (code == War3ID("Adri") || code == War3ID("Astd")) {
        return std::make_unique<CAbilityDropInstant>(handleId, code);
    }
    if (code == War3ID("Awrp")) return std::make_unique<CAbilityWayGate>(handleId);
    if (code == War3ID("Apit")) return std::make_unique<CAbilityShopPurchase>(handleId);
    if (code == War3ID("Aneu")) {
        return std::make_unique<CAbilityNeutralBuilding>(handleId, code, false);
    }
    if (code == War3ID("Aall")) {
        return std::make_unique<CAbilityNeutralBuilding>(handleId, code, true);
    }

    // ===== Batch 29: entangle mine, channel test =====
    if (code == War3ID("Aent")) return std::make_unique<CAbilityEntangleGoldMine>(handleId);
    if (code == War3ID("Aegm")) return std::make_unique<CAbilityEntangledMine>(handleId);
    if (code == War3ID("Aenc")) {
        return std::make_unique<CAbilityCargoHold>(handleId, code, false, 5);
    }
    if (code == War3ID("ANcl")) return std::make_unique<CAbilityChannelTest>(handleId);

    // ===== Auras =====
    if (code == War3ID("AHad")) return std::make_unique<CAbilityDevotionAura>(handleId);
    if (code == War3ID("AHab")) return std::make_unique<CAbilityBrillianceAura>(handleId);
    if (code == War3ID("AOae")) return std::make_unique<CAbilityEnduranceAura>(handleId);
    if (code == War3ID("AOac")) return std::make_unique<CAbilityCommandAura>(handleId);
    if (code == War3ID("AUau")) return std::make_unique<CAbilityUnholyAura>(handleId);
    if (code == War3ID("AUav")) return std::make_unique<CAbilityVampiricAura>(handleId);
    if (code == War3ID("AEah")) return std::make_unique<CAbilityThornsAura>(handleId);
    if (code == War3ID("AEar")) return std::make_unique<CAbilityTrueshotAura>(handleId);

    // Unknown ability
    return nullptr;
}

void AbilityFactory::AddDefaultAbilities(CSimulation& game, CUnit* unit,
                                          const std::vector<War3ID>& abilityList) {
    // Add core abilities (attack, move)
    int handleId = 0; // Will be assigned by CSimulation
    (void)handleId;

    // Add abilities from the unit type's ability list
    for (auto& code : abilityList) {
        // Skip abilities that are added by default (attack, move)
        if (code == War3ID("Aatk") || code == War3ID("Amov")) continue;

        // Check if unit already has this ability
        if (unit->GetAbility(code)) continue;

        // Create the ability
        // Note: handleId allocation is handled by CSimulation
        // For now, we skip abilities that need handle IDs
        // TODO: integrate with CSimulation's handle allocator
        (void)game;
    }
}

void AbilityFactory::AddQueueAbilities(CSimulation& game, CUnit* unit,
                                        const std::vector<War3ID>& unitsTrained,
                                        const std::vector<War3ID>& researchesAvailable) {
    (void)game;
    // Queue ability is already added by CSimulation for buildings
    // Populate the trained units and researches
    auto* queue = dynamic_cast<CAbilityQueue*>(unit->GetAbility(War3ID("Aque")));
    if (queue) {
        for (auto& id : unitsTrained) {
            queue->AddTrainedUnit(id);
        }
        for (auto& id : researchesAvailable) {
            queue->AddResearch(id);
        }
    }
}
