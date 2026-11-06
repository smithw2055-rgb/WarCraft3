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
#include "ecs/abilities/ability_generic.h"
#include "ecs/abilities/ability_root.h"
#include "ecs/abilities/ability_spell_base.h"
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
#include <unordered_map>

namespace {

struct RegistryStore {
    std::unordered_map<uint32_t, AbilityFactory::CreatorFn> creators;
    bool initialized = false;
};

RegistryStore& GetStore() {
    static RegistryStore store;
    if (!store.initialized) {
        store.initialized = true;
        auto& map = store.creators;

        auto regSimple = [&map](const char* raw, auto makeFn) {
            map[War3ID(raw).value] = [makeFn](int h, War3ID) -> std::unique_ptr<CAbility> {
                return makeFn(h);
            };
        };
        auto regAliased = [&map](const char* raw, auto makeFn) {
            map[War3ID(raw).value] = [makeFn](int h, War3ID code) -> std::unique_ptr<CAbility> {
                auto ab = makeFn(h);
                if (ab) ab->SetAlias(code);
                return ab;
            };
        };
        auto regWithCode = [&map](const char* raw, auto makeFn) {
            map[War3ID(raw).value] = [makeFn](int h, War3ID code) -> std::unique_ptr<CAbility> {
                return makeFn(h, code);
            };
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
    }
    return store;
}

} // namespace

void AbilityFactory::RegisterCreator(War3ID code, CreatorFn creator) {
    GetStore().creators[code.value] = std::move(creator);
}

void AbilityFactory::RegisterAlias(War3ID alias, War3ID targetCode) {
    auto& creators = GetStore().creators;
    auto it = creators.find(targetCode.value);
    if (it != creators.end()) {
        CreatorFn targetFn = it->second;
        creators[alias.value] = [targetFn, targetCode](int handleId, War3ID aliasCode) -> std::unique_ptr<CAbility> {
            auto ab = targetFn(handleId, targetCode);
            if (ab) ab->SetAlias(aliasCode);
            return ab;
        };
    }
}

void AbilityFactory::RegisterSpellDescriptor(const ABSpellDescriptor& desc) {
    AbilityBuilderRegistry::Instance().RegisterDescriptor(desc);
    GetStore().creators[desc.code.value] = [desc](int handleId, War3ID code) -> std::unique_ptr<CAbility> {
        auto spell = std::make_unique<CAbilityDataDrivenSpell>(handleId, code, desc);
        spell->SetAlias(code);
        return spell;
    };
}

bool AbilityFactory::HasRegisteredCreator(War3ID code) {
    if (GetStore().creators.count(code.value) > 0) return true;
    if (AbilityBuilderRegistry::Instance().FindDescriptor(code) != nullptr) return true;
    if (AbilityBuilderRegistry::Instance().Find(code) != nullptr) return true;
    return false;
}

size_t AbilityFactory::GetRegisteredCount() {
    return GetStore().creators.size();
}

std::unique_ptr<CAbility> AbilityFactory::CreateAbility(int handleId, War3ID code) {
    // 1. Check O(1) registered creator table
    auto& creators = GetStore().creators;
    auto it = creators.find(code.value);
    if (it != creators.end() && it->second) {
        return it->second(handleId, code);
    }

    // 2. Check data-driven ABSpellDescriptor registry
    if (const ABSpellDescriptor* desc = AbilityBuilderRegistry::Instance().FindDescriptor(code)) {
        auto spell = std::make_unique<CAbilityDataDrivenSpell>(handleId, code, *desc);
        spell->SetAlias(code);
        return spell;
    }

    // 3. Check Spells22 alias table
    if (auto alias22 = CreateSpells22Alias(handleId, code)) {
        return alias22;
    }

    // 4. Check raw ABEffectChain registry
    if (AbilityBuilderRegistry::Instance().Find(code) != nullptr) {
        ABSpellDescriptor desc;
        desc.code = code;
        auto spell = std::make_unique<CAbilityDataDrivenSpell>(handleId, code, desc);
        spell->SetAlias(code);
        return spell;
    }

    // 5. Fallback generic do-nothing placeholder
    return std::make_unique<CAbilityGenericDoNothing>(handleId, code);
}

void AbilityFactory::AddDefaultAbilities(CSimulation& game, CUnit* unit,
                                         const std::vector<War3ID>& abilityList) {
    if (!unit) return;
    for (auto& code : abilityList) {
        if (code == War3ID("Aatk") || code == War3ID("Amov")) continue;
        if (unit->GetAbility(code)) continue;
        std::unique_ptr<CAbility> ability =
            CreateAbility(game.AllocateHandleId(), code);
        if (!ability) continue;
        unit->AddAbility(game, std::move(ability));
    }
}

void AbilityFactory::AddQueueAbilities(CSimulation& game, CUnit* unit,
                                       const std::vector<War3ID>& unitsTrained,
                                       const std::vector<War3ID>& researchesAvailable) {
    (void)game;
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
