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
    if (code == War3ID("AEtr")) return std::make_unique<CAbilityTranquility>(handleId);
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
