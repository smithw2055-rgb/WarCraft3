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
    if (code == War3ID("AHre")) return std::make_unique<CAbilitySerenity>(handleId);

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
    if (code == War3ID("AHrs")) return std::make_unique<CAbilityResurrect>(handleId);
    if (code == War3ID("AEbl")) return std::make_unique<CAbilityBlink>(handleId);
    if (code == War3ID("AOmi")) return std::make_unique<CAbilityMirrorImage>(handleId);
    if (code == War3ID("AOsh")) return std::make_unique<CAbilityShockwave>(handleId);
    if (code == War3ID("AEst")) return std::make_unique<CAbilityStarfall>(handleId);
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
