#include <iostream>
#include <string>
#include <vector>

using TestFn = bool(*)();

struct TestCase {
    const char* name;
    TestFn fn;
};

bool RunTest(const TestCase& tc) {
    const bool ok = tc.fn();
    std::cout << (ok ? "[PASS] " : "[FAIL] ") << tc.name << std::endl;
    return ok;
}

bool TestW3ELoadValidMinimal();
bool TestW3ELoadRejectsBadMagic();
bool TestDecodeBLPRejectsBadMagic();
bool TestDecodeBLPDecodesMinimalPaletted();
bool TestMdxTimelineSamplesLinearValues();
bool TestMdxTimelineRejectsShortValues();
bool TestMdxTimelineFallsBackWhenTangentsAreMissing();
bool TestMdxTimelineUsesBezierTangents();
bool TestMdxModelTimelineSamplingIgnoresOtherSequences();
bool TestMdxAnimatedGeosetColorConvertsBgrToRgb();
bool TestMdxParsesLayerTextureIdTimelineAsInteger();
bool TestMdxParsesLightTypeAndTimelines();
bool TestMdxSamplesCameraTimelinesOnlyInsideCurrentSequence();
bool TestMdxParsesRibbonColorAndTextureSlotTimelines();
bool TestMdxSamplesParticleEmitter2RuntimeTimelines();
bool TestMdxSamplesParticleEmitter2TimelinesOnlyInsideCurrentSequence();
bool TestMdxSamplesParticleEmitter1TimelinesOnlyInsideCurrentSequence();
bool TestMdxCollectsParticleEmitter1ModelSpawns();
bool TestMdxAdvancesParticleEmitter1SpawnWithGravity();
bool TestMdxCollectsParticleEmitter1OnlyWhenVisibleInCurrentSequence();
bool TestMdxAnimatorSamplesParticleEmitter2Visibility();
bool TestMdxParsesEventObjectKeyframes();
bool TestMdxParsesCollisionShapeAndAnimatorNode();
bool TestMdxParsesPopcornEmitterAndAnimatorNode();
bool TestMdxCollisionSphereRayUsesInstanceTransform();
bool TestMdxCollisionBoxRayUsesCollisionExtents();
bool TestMdxAnimatorSamplesGlobalSequenceBones();
bool TestMdxAnimatorLoopRestartMatchesReferenceEndFrame();
bool TestMdxAnimatorIgnoresSequenceLocalBoneKeysOutsideCurrentSequence();
bool TestMdxAnimatorSamplesAttachmentVisibilityAsStep();
bool TestMdxAnimatorSamplesLightVisibilityAsStep();
bool TestMdxAnimatorUsesBoneGeosetAnimationVisibility();
bool TestMdxAnimatorTriggersEventKeyframes();
bool TestMdxAnimatorResetsEventKeyframesOnLoop();
bool TestMdxAnimatorTriggersGlobalSequenceEvents();
bool TestMdxAnimatorEventTriggersCarryNodeTransform();
bool TestMdxAnimatorEventTriggerUsesPivotWorldLocation();
bool TestMdxEventSplatMeshUsesWorldTransform();
bool TestMdxEventSplatMeshCancelsNodeScale();
bool TestMdxEventSplatScaleCompensationMatchesEventType();
bool TestMdxSequenceSelectsExactSecondaryTags();
bool TestMdxSequenceSelectsSpellSecondaryFallback();
bool TestMdxSequenceDurationSeconds();
bool TestMdxSkipsFaceFxAndBindPose();
bool TestJassAnimationTagOrdinalsMatchReference();
bool TestJassPopulateAnimationTagsFromSelector();
bool TestJassVmThreadSleepResumesAfterWait();
bool TestItemDamageArmorApplyRemoveSymmetric();
bool TestItemStrengthFlowsThroughHeroAbility();
bool TestUpgradeEffectsApplyAndUnapply();
bool TestSlowAndBloodlustRestoreSpeed();
bool TestVampiricAndUnholyAuraIdsDiffer();
bool TestAuraArmorDamageRegenAttackSpeedSymmetric();
bool TestDivineShieldStackingKeepsInvulnerable();
bool TestRegionEnterLeaveFiresOnMigration();
bool TestLifecycleEventsAndPlayerUnitDualMatch();
bool TestEnumDestructablesInRect();
bool TestSplashDamagesEnemiesNotAllies();
bool TestProjectileBounceChainsWithFalloff();
bool TestEvasionNegatesAttacks();
bool TestCriticalStrikeMultipliesDamage();
bool TestAbilityAndBuffMapMods();
bool TestUpgradeMapModsAndBaseCosts();
bool TestThornsAuraGrantsReflectBuff();
bool TestVampiricAuraHealsAttacker();
bool TestVampiricAuraViaAuraAbility();
bool TestRootUprootSwapAndHold();
bool TestNightElfBuildConsumesWisp();
bool TestHumanBuildPausesWhenWorkerLeaves();
bool TestForGroupSleepDoesNotPoisonVm();
bool TestThornsReflectsOnceWithoutPingPong();
bool TestStunParksAndClearsBehavior();
bool TestAttackDefenseTableMatchesOfficialChart();
bool TestUnitDamageAppliesArmorTypeMultiplier();
bool TestNonNormalDamageTypeIgnoresArmorValue();
bool TestNegativeArmorAmplifiesDamage();
bool TestBonusDamageIsMultipliedNotBypassing();
bool TestCombatSystemBlockedMathConsistent();
bool TestItemLifeSurvivesHeroRecalc();
bool TestUpgradeLifeSurvivesHeroLevelUp();
bool TestMaxLossClampsLifeButGainDoesNotHeal();
bool TestManaLayersStack();
bool TestNonHeroFallbackStacksWithItemAndUpgrade();
bool TestMiscDataTableAndArmorParsed();
bool TestSpellsRowFallsBackToMagic();
bool TestMalformedRowKeepsDefaults();
bool TestConfiguredTableDrivesDamage();
bool TestConfiguredDefenseArmorDrivesReduction();
bool TestWeaponRangedClassification();
bool TestProjectileHitsAfterTravelTime();
bool TestProjectileFizzlesOnDeadTarget();
bool TestProjectileHitsWithoutAttacker();
bool TestProjectileMaxTravelGuard();
bool TestLoadInRangeIsInstant();
bool TestLoadOutOfRangeWalksThenLoads();
bool TestLoadCapacityEnforced();
bool TestUnloadRestoresUnits();
bool TestCargoFollowsTransport();
bool TestTransportDeathKillsCargo();
bool TestJassHeroXpAndStats();
bool TestJassUnitRaceAndIdType();
bool TestJassPlayerIdentityAndLocationZ();
bool TestJassGameStateAndConfig();
bool TestJassUnitMobilityAndWaygate();
bool TestJassDialogBoards();
bool TestJassCameraSetup();
bool TestJassSoundMusic();
bool TestJassCinematicVisuals();
bool TestJassSuspendDecayMisc();
bool TestTimedLifeKillsBearerOnExpiry();
bool TestMassTeleportMovesGroup();
bool TestBlinkClampsRange();
bool TestShockwaveLineDamage();
bool TestStarfallDamagesArea();
bool TestTranquilityHealsAllies();
bool TestResurrectRevivesInRadius();
bool TestReincarnationRevivesOnDeath();
bool TestCharmConvertsCreep();
bool TestSummonWaterElemental();
bool TestMirrorImageCopiesCaster();
bool TestSpells3FactoryCoverage();
bool TestEvasionPassive();
bool TestCriticalStrikePassive();
bool TestBashProcsOnHit();
bool TestSlowAppliesDebuff();
bool TestFrostArmorAndFaerieFire();
bool TestDeathPactHeals();
bool TestAnimateDeadRaisesInvulnerable();
bool TestDoomDotsAndSpawnsGuard();
bool TestVolcanoErupts();
bool TestSpells4FactoryCoverage();
bool TestFlameStrikeWaves();
bool TestDeathAndDecayHitsAll();
bool TestImpaleLineStuns();
bool TestFanOfKnivesHitsEnemiesOnly();
bool TestRainOfFireWaves();
bool TestSpikedCarapaceArmorAndReflect();
bool TestDarkRitualGrantsMana();
bool TestShadowStrikeSlowsAndPoisons();
bool TestSpells5FactoryCoverage();
bool TestHealingSprayHealsAllies();
bool TestAcidBombCurses();
bool TestChemicalRageHastes();
bool TestTransmutePaysGold();
bool TestBloodlustCastHastes();
bool TestPurgeDispelsAndSlows();
bool TestPurgeBurnsSummons();
bool TestLightningShieldZaps();
bool TestRaiseDeadSkeletons();
bool TestSpells6FactoryCoverage();
bool TestSerpentWardSummons();
bool TestPolymorphHexes();
bool TestInnerFireBuffs();
bool TestPriestHealHeals();
bool TestDispelMagicStripsArea();
bool TestRejuvenationHealsOverTime();
bool TestRoarRalliesAllies();
bool TestDefendHalvesAttacks();
bool TestSpells7FactoryCoverage();
bool TestHexUsesMarkerNotStun();
bool TestSleepWakesOnDamage();
bool TestSilenceBlocksCastOnly();
bool TestLifeDrainTicksHealCaster();
bool TestEffectChainDamageAndHeal();
bool TestBuilderRegistryRoundTrip();
bool TestSimRandIsDeterministic();
bool TestSummonWithoutUnitDataIsGraceful();
bool TestHeroSkillLevelsAndGrants();
bool TestVfsFolderLastAddedWins();
bool TestVfsFolderShadowsArchive();
bool TestVfsLastOpenedArchiveWins();
bool TestW3iParsesPlayersAndForces();
bool TestW3iParsesTechAndRandomTables();
bool TestUnitsDooRetainsDropsAndInventory();
bool TestIssueOrderByIdNatives();
bool TestTerrainQueryNatives();
bool TestRectRegionCellNatives();
bool TestPlayerTeamNatives();
bool TestAbilityArtLookupFromProfiles();
bool TestSpellEffectNatives();
bool TestImageNatives();
bool TestTrackableAndUbersplatNatives();
bool TestPoolPlaceRandomItem();
bool TestPoolPlaceRandomUnit();
bool TestChooseRandomCreep();
bool TestShopStockStateMachine();
bool TestShopPurchaseConsumesStock();
bool TestUnitTypeFlagsAndPermanent();
bool TestUnitTimedLifePause();
bool TestUnitSleepFamily();
bool TestUnitAlarmIconProgress();
bool TestUnitBuffExFamily();
bool TestUnitDamagePointDelayed();
bool TestUnitIndicatorVisionFog();
bool TestCameraFlowNatives();
bool TestGameFlowNatives();
bool TestTriggerExtraNatives();
bool TestSoundStateNatives();
bool TestGroupEnumCountedNatives();
bool TestUIStateNatives();
bool TestLightningAndIndicatorNatives();
bool TestLeaderboardNatives();
bool TestMultiboardNatives();
bool TestTimerDialogStyleNatives();
bool TestMeleeAIDiscoversHumanPlan();
bool TestMeleeAIEconomyGrows();
bool TestMeleeAIAttackWave();
bool TestMeleeAINativesWiring();
bool TestMdxParsesRibbonEmitter();
bool TestRibbonEmissionAndQuads();
bool TestRibbonAboveBelowGeometry();
bool TestRibbonAgeOut();
bool TestRibbonGravitySinksVerts();
bool TestRibbonParseAndAnimateRealModel();
bool TestStructAllocAndMembers();
bool TestStructMethodsAndThis();
bool TestStructStaticAndCreate();
bool TestStructExtends();
bool TestStructArrayMembers();
bool TestStructDestroyAndNull();
bool TestScopePrivacyAndInitializer();
bool TestLibraryWrapper();
bool TestLeadingDotAndMethodRef();
bool TestStructMethodRefValue();
bool TestModuleBasicImplement();
bool TestModuleStaticPerStruct();
bool TestModuleOverrideAndOptional();
bool TestModuleForwardReference();
bool TestModuleMissingError();
bool TestSetTerrainType();
bool TestHashtableValues();
bool TestHashtableHandles();
bool TestGameCacheValuesAndFlush();
bool TestGameCacheDiskPersistence();
bool TestGameCacheStoreRestoreUnit();
bool TestReviveHero();
bool TestAbilityLevelShift();
bool TestSelectUnitAndFood();
bool TestDestructableNatives();
bool TestCampaignAndFlowNatives();
bool TestUnitPointValue();
bool TestSetTerrainPathable();
bool TestWaterDeformsAndDoodadAnim();
bool TestTerrainDeformRegistry();
#ifndef WITHOUT_STORMLIB
bool RunStormLibTests();
#endif

int main() {
    std::vector<TestCase> tests = {
        {"W3E loads minimal valid payload", TestW3ELoadValidMinimal},
        {"W3E rejects invalid magic", TestW3ELoadRejectsBadMagic},
        {"BLP rejects invalid magic", TestDecodeBLPRejectsBadMagic},
        {"BLP decodes minimal paletted texture", TestDecodeBLPDecodesMinimalPaletted},
        {"MDX timeline samples linear values", TestMdxTimelineSamplesLinearValues},
        {"MDX timeline rejects short value arrays", TestMdxTimelineRejectsShortValues},
        {"MDX timeline falls back without tangents", TestMdxTimelineFallsBackWhenTangentsAreMissing},
        {"MDX timeline uses Bezier tangents", TestMdxTimelineUsesBezierTangents},
        {"MDX model timeline sampling ignores other sequences", TestMdxModelTimelineSamplingIgnoresOtherSequences},
        {"MDX animated geoset color converts BGR to RGB", TestMdxAnimatedGeosetColorConvertsBgrToRgb},
        {"MDX parses layer texture id timeline as integer", TestMdxParsesLayerTextureIdTimelineAsInteger},
        {"MDX parses light type and timelines", TestMdxParsesLightTypeAndTimelines},
        {"MDX samples camera timelines only inside current sequence", TestMdxSamplesCameraTimelinesOnlyInsideCurrentSequence},
        {"MDX parses ribbon color and texture slot timelines", TestMdxParsesRibbonColorAndTextureSlotTimelines},
        {"MDX samples particle emitter 2 runtime timelines", TestMdxSamplesParticleEmitter2RuntimeTimelines},
        {"MDX samples particle emitter 2 timelines only inside current sequence", TestMdxSamplesParticleEmitter2TimelinesOnlyInsideCurrentSequence},
        {"MDX samples particle emitter 1 timelines only inside current sequence", TestMdxSamplesParticleEmitter1TimelinesOnlyInsideCurrentSequence},
        {"MDX collects particle emitter 1 model spawns", TestMdxCollectsParticleEmitter1ModelSpawns},
        {"MDX advances particle emitter 1 spawn with gravity", TestMdxAdvancesParticleEmitter1SpawnWithGravity},
        {"MDX collects particle emitter 1 only when visible in current sequence", TestMdxCollectsParticleEmitter1OnlyWhenVisibleInCurrentSequence},
        {"MDX animator samples particle emitter 2 visibility", TestMdxAnimatorSamplesParticleEmitter2Visibility},
        {"MDX parses event object keyframes", TestMdxParsesEventObjectKeyframes},
        {"MDX parses collision shape and animator node", TestMdxParsesCollisionShapeAndAnimatorNode},
        {"MDX parses Popcorn emitter and animator node", TestMdxParsesPopcornEmitterAndAnimatorNode},
        {"MDX collision sphere ray uses instance transform", TestMdxCollisionSphereRayUsesInstanceTransform},
        {"MDX collision box ray uses collision extents", TestMdxCollisionBoxRayUsesCollisionExtents},
        {"MDX animator samples global-sequence bones", TestMdxAnimatorSamplesGlobalSequenceBones},
        {"MDX animator loop restart matches reference end frame", TestMdxAnimatorLoopRestartMatchesReferenceEndFrame},
        {"MDX animator ignores sequence-local bone keys outside current sequence", TestMdxAnimatorIgnoresSequenceLocalBoneKeysOutsideCurrentSequence},
        {"MDX animator samples attachment visibility as step", TestMdxAnimatorSamplesAttachmentVisibilityAsStep},
        {"MDX animator samples light visibility as step", TestMdxAnimatorSamplesLightVisibilityAsStep},
        {"MDX animator uses bone geoset animation visibility", TestMdxAnimatorUsesBoneGeosetAnimationVisibility},
        {"MDX animator triggers event keyframes", TestMdxAnimatorTriggersEventKeyframes},
        {"MDX animator resets event keyframes on loop", TestMdxAnimatorResetsEventKeyframesOnLoop},
        {"MDX animator triggers global-sequence events", TestMdxAnimatorTriggersGlobalSequenceEvents},
        {"MDX animator event triggers carry node transform", TestMdxAnimatorEventTriggersCarryNodeTransform},
        {"MDX animator event trigger uses pivot world location", TestMdxAnimatorEventTriggerUsesPivotWorldLocation},
        {"MDX event splat mesh uses world transform", TestMdxEventSplatMeshUsesWorldTransform},
        {"MDX event splat mesh cancels node scale", TestMdxEventSplatMeshCancelsNodeScale},
        {"MDX event splat scale compensation matches event type", TestMdxEventSplatScaleCompensationMatchesEventType},
        {"MDX sequence selects exact secondary tags", TestMdxSequenceSelectsExactSecondaryTags},
        {"MDX sequence selects spell secondary fallback", TestMdxSequenceSelectsSpellSecondaryFallback},
        {"MDX sequence duration seconds", TestMdxSequenceDurationSeconds},
        {"MDX skips FaceFX and bind pose", TestMdxSkipsFaceFxAndBindPose},
        {"JASS animation tag ordinals match reference", TestJassAnimationTagOrdinalsMatchReference},
        {"JASS PopulateTags parses selector", TestJassPopulateAnimationTagsFromSelector},
        {"JASS VM thread sleep resumes after wait", TestJassVmThreadSleepResumesAfterWait},
        {"Item damage/armor apply/remove symmetric", TestItemDamageArmorApplyRemoveSymmetric},
        {"Item strength flows through hero ability", TestItemStrengthFlowsThroughHeroAbility},
        {"Upgrade effects apply and unapply", TestUpgradeEffectsApplyAndUnapply},
        {"Slow and bloodlust restore speed", TestSlowAndBloodlustRestoreSpeed},
        {"Vampiric and unholy aura ids differ", TestVampiricAndUnholyAuraIdsDiffer},
        {"Aura armor/regen/attack-speed symmetric", TestAuraArmorDamageRegenAttackSpeedSymmetric},
        {"Divine shield stacking keeps invulnerable", TestDivineShieldStackingKeepsInvulnerable},
        {"Region enter/leave fires on migration", TestRegionEnterLeaveFiresOnMigration},
        {"Lifecycle events and player-unit dual match", TestLifecycleEventsAndPlayerUnitDualMatch},
        {"Enum destructables in rect", TestEnumDestructablesInRect},
        {"Splash damages enemies not allies", TestSplashDamagesEnemiesNotAllies},
        {"Projectile bounce chains with falloff", TestProjectileBounceChainsWithFalloff},
        {"Evasion negates attacks", TestEvasionNegatesAttacks},
        {"Critical strike multiplies damage", TestCriticalStrikeMultipliesDamage},
        {"Ability and buff map mods", TestAbilityAndBuffMapMods},
        {"Upgrade map mods and base costs", TestUpgradeMapModsAndBaseCosts},
        {"Thorns aura grants reflect buff", TestThornsAuraGrantsReflectBuff},
        {"Vampiric aura heals attacker", TestVampiricAuraHealsAttacker},
        {"Vampiric aura via aura ability", TestVampiricAuraViaAuraAbility},
        {"Root/uproot swap and hold", TestRootUprootSwapAndHold},
        {"Night elf build consumes wisp", TestNightElfBuildConsumesWisp},
        {"Human build pauses when worker leaves", TestHumanBuildPausesWhenWorkerLeaves},
        {"ForGroup sleep does not poison VM", TestForGroupSleepDoesNotPoisonVm},
        {"Thorns reflects once without ping-pong", TestThornsReflectsOnceWithoutPingPong},
        {"Stun parks and clears behavior", TestStunParksAndClearsBehavior},
        {"Attack/defense table matches official chart", TestAttackDefenseTableMatchesOfficialChart},
        {"Unit damage applies armor-type multiplier", TestUnitDamageAppliesArmorTypeMultiplier},
        {"Non-normal damage ignores armor value", TestNonNormalDamageTypeIgnoresArmorValue},
        {"Negative armor amplifies damage", TestNegativeArmorAmplifiesDamage},
        {"Bonus damage is multiplied, not bypassing", TestBonusDamageIsMultipliedNotBypassing},
        {"Combat blocked math consistent", TestCombatSystemBlockedMathConsistent},
        {"Item life survives hero recalc", TestItemLifeSurvivesHeroRecalc},
        {"Upgrade life survives hero level-up", TestUpgradeLifeSurvivesHeroLevelUp},
        {"Max loss clamps life, gain does not heal", TestMaxLossClampsLifeButGainDoesNotHeal},
        {"Mana layers stack", TestManaLayersStack},
        {"Non-hero fallback stacks with item and upgrade", TestNonHeroFallbackStacksWithItemAndUpgrade},
        {"Misc data table and armor parsed", TestMiscDataTableAndArmorParsed},
        {"Spells row falls back to magic", TestSpellsRowFallsBackToMagic},
        {"Malformed row keeps defaults", TestMalformedRowKeepsDefaults},
        {"Configured table drives damage", TestConfiguredTableDrivesDamage},
        {"Configured defense armor drives reduction", TestConfiguredDefenseArmorDrivesReduction},
        {"Weapon ranged classification", TestWeaponRangedClassification},
        {"Projectile hits after travel time", TestProjectileHitsAfterTravelTime},
        {"Projectile fizzles on dead target", TestProjectileFizzlesOnDeadTarget},
        {"Projectile hits without attacker", TestProjectileHitsWithoutAttacker},
        {"Projectile max-travel guard", TestProjectileMaxTravelGuard},
        {"Load in range is instant", TestLoadInRangeIsInstant},
        {"Load out of range walks then loads", TestLoadOutOfRangeWalksThenLoads},
        {"Load capacity enforced", TestLoadCapacityEnforced},
        {"Unload restores units", TestUnloadRestoresUnits},
        {"Cargo follows transport", TestCargoFollowsTransport},
        {"Transport death kills cargo", TestTransportDeathKillsCargo},
        {"JASS hero XP and stats", TestJassHeroXpAndStats},
        {"JASS unit race and id-type", TestJassUnitRaceAndIdType},
        {"JASS player identity and location Z", TestJassPlayerIdentityAndLocationZ},
        {"JASS game state and config", TestJassGameStateAndConfig},
        {"JASS unit mobility and waygate", TestJassUnitMobilityAndWaygate},
        {"JASS dialog boards", TestJassDialogBoards},
        {"JASS camera setup", TestJassCameraSetup},
        {"JASS sound music", TestJassSoundMusic},
        {"JASS cinematic visuals", TestJassCinematicVisuals},
        {"JASS suspend decay misc", TestJassSuspendDecayMisc},
        {"Timed life kills bearer on expiry", TestTimedLifeKillsBearerOnExpiry},
        {"Mass teleport moves group", TestMassTeleportMovesGroup},
        {"Blink clamps range", TestBlinkClampsRange},
        {"Shockwave line damage", TestShockwaveLineDamage},
        {"Starfall damages area", TestStarfallDamagesArea},
        {"Tranquility heals allies", TestTranquilityHealsAllies},
        {"Resurrect revives in radius", TestResurrectRevivesInRadius},
        {"Reincarnation revives on death", TestReincarnationRevivesOnDeath},
        {"Charm converts creep", TestCharmConvertsCreep},
        {"Summon water elemental", TestSummonWaterElemental},
        {"Mirror image copies caster", TestMirrorImageCopiesCaster},
        {"Spells3 factory coverage", TestSpells3FactoryCoverage},
        {"Evasion passive", TestEvasionPassive},
        {"Critical strike passive", TestCriticalStrikePassive},
        {"Bash procs on hit", TestBashProcsOnHit},
        {"Slow applies debuff", TestSlowAppliesDebuff},
        {"Frost armor and faerie fire", TestFrostArmorAndFaerieFire},
        {"Death pact heals", TestDeathPactHeals},
        {"Animate dead raises invulnerable", TestAnimateDeadRaisesInvulnerable},
        {"Doom DoTs and spawns guard", TestDoomDotsAndSpawnsGuard},
        {"Volcano erupts", TestVolcanoErupts},
        {"Spells4 factory coverage", TestSpells4FactoryCoverage},
        {"Flame strike waves", TestFlameStrikeWaves},
        {"Death and decay hits all", TestDeathAndDecayHitsAll},
        {"Impale line stuns", TestImpaleLineStuns},
        {"Fan of knives hits enemies only", TestFanOfKnivesHitsEnemiesOnly},
        {"Rain of fire waves", TestRainOfFireWaves},
        {"Spiked carapace armor and reflect", TestSpikedCarapaceArmorAndReflect},
        {"Dark ritual grants mana", TestDarkRitualGrantsMana},
        {"Shadow strike slows and poisons", TestShadowStrikeSlowsAndPoisons},
        {"Spells5 factory coverage", TestSpells5FactoryCoverage},
        {"Healing spray heals allies", TestHealingSprayHealsAllies},
        {"Acid bomb curses", TestAcidBombCurses},
        {"Chemical rage hastes", TestChemicalRageHastes},
        {"Transmute pays gold", TestTransmutePaysGold},
        {"Bloodlust cast hastes", TestBloodlustCastHastes},
        {"Purge dispels and slows", TestPurgeDispelsAndSlows},
        {"Purge burns summons", TestPurgeBurnsSummons},
        {"Lightning shield zaps", TestLightningShieldZaps},
        {"Raise dead skeletons", TestRaiseDeadSkeletons},
        {"Spells6 factory coverage", TestSpells6FactoryCoverage},
        {"Serpent ward summons", TestSerpentWardSummons},
        {"Polymorph hexes", TestPolymorphHexes},
        {"Inner fire buffs", TestInnerFireBuffs},
        {"Priest heal heals", TestPriestHealHeals},
        {"Dispel magic strips area", TestDispelMagicStripsArea},
        {"Rejuvenation heals over time", TestRejuvenationHealsOverTime},
        {"Roar rallies allies", TestRoarRalliesAllies},
        {"Defend halves attacks", TestDefendHalvesAttacks},
        {"Spells7 factory coverage", TestSpells7FactoryCoverage},
        {"Hex uses marker not stun", TestHexUsesMarkerNotStun},
        {"Sleep wakes on damage", TestSleepWakesOnDamage},
        {"Silence blocks cast only", TestSilenceBlocksCastOnly},
        {"Life drain ticks heal caster", TestLifeDrainTicksHealCaster},
        {"Effect chain damage and heal", TestEffectChainDamageAndHeal},
        {"Builder registry round trip", TestBuilderRegistryRoundTrip},
        {"Sim rand is deterministic", TestSimRandIsDeterministic},
        {"Summon without unit data is graceful", TestSummonWithoutUnitDataIsGraceful},
        {"Hero skill levels and grants", TestHeroSkillLevelsAndGrants},
        {"VFS folder last-added wins", TestVfsFolderLastAddedWins},
        {"VFS folder shadows archive", TestVfsFolderShadowsArchive},
        {"VFS game archive order and map mount", TestVfsLastOpenedArchiveWins},
        {"W3I parses players and forces", TestW3iParsesPlayersAndForces},
        {"W3I parses tech and random tables", TestW3iParsesTechAndRandomTables},
        {"Units.doo retains drops and inventory", TestUnitsDooRetainsDropsAndInventory},
        {"Issue order-by-id natives", TestIssueOrderByIdNatives},
        {"Terrain query natives", TestTerrainQueryNatives},
        {"Rect/region cell natives", TestRectRegionCellNatives},
        {"Player/team natives", TestPlayerTeamNatives},
        {"Ability art lookup from profiles", TestAbilityArtLookupFromProfiles},
        {"Spell effect natives", TestSpellEffectNatives},
        {"Image natives", TestImageNatives},
        {"Trackable and ubersplat natives", TestTrackableAndUbersplatNatives},
        {"Pool place random item", TestPoolPlaceRandomItem},
        {"Pool place random unit", TestPoolPlaceRandomUnit},
        {"Choose random creep", TestChooseRandomCreep},
        {"Shop stock state machine", TestShopStockStateMachine},
        {"Shop purchase consumes stock", TestShopPurchaseConsumesStock},
        {"Unit type flags and permanent", TestUnitTypeFlagsAndPermanent},
        {"Unit timed life pause", TestUnitTimedLifePause},
        {"Unit sleep family", TestUnitSleepFamily},
        {"Unit alarm/icon/progress", TestUnitAlarmIconProgress},
        {"Unit BuffEx family", TestUnitBuffExFamily},
        {"Unit damage point delayed", TestUnitDamagePointDelayed},
        {"Unit indicator/vision/fog", TestUnitIndicatorVisionFog},
        {"Camera flow natives", TestCameraFlowNatives},
        {"Game flow natives", TestGameFlowNatives},
        {"Trigger extra natives", TestTriggerExtraNatives},
        {"Sound state natives", TestSoundStateNatives},
        {"Group enum counted natives", TestGroupEnumCountedNatives},
        {"UI state natives", TestUIStateNatives},
        {"Lightning and indicator natives", TestLightningAndIndicatorNatives},
        {"Leaderboard natives", TestLeaderboardNatives},
        {"Multiboard natives", TestMultiboardNatives},
        {"Timer dialog style natives", TestTimerDialogStyleNatives},
        {"Melee AI discovers human plan", TestMeleeAIDiscoversHumanPlan},
        {"Melee AI economy grows", TestMeleeAIEconomyGrows},
        {"Melee AI attack wave", TestMeleeAIAttackWave},
        {"Melee AI natives wiring", TestMeleeAINativesWiring},
        {"MDX parses ribbon emitter", TestMdxParsesRibbonEmitter},
        {"Ribbon emission and quads", TestRibbonEmissionAndQuads},
        {"Ribbon above/below geometry", TestRibbonAboveBelowGeometry},
        {"Ribbon age out", TestRibbonAgeOut},
        {"Ribbon gravity sinks verts", TestRibbonGravitySinksVerts},
        {"Ribbon parse and animate real model", TestRibbonParseAndAnimateRealModel},
        {"Struct alloc and members", TestStructAllocAndMembers},
        {"Struct methods and this", TestStructMethodsAndThis},
        {"Struct static and create", TestStructStaticAndCreate},
        {"Struct extends", TestStructExtends},
        {"Struct array members", TestStructArrayMembers},
        {"Struct destroy and null", TestStructDestroyAndNull},
        {"Scope privacy and initializer", TestScopePrivacyAndInitializer},
        {"Library wrapper", TestLibraryWrapper},
        {"Leading dot and method ref", TestLeadingDotAndMethodRef},
        {"Struct method ref value", TestStructMethodRefValue},
        {"Module basic implement", TestModuleBasicImplement},
        {"Module static per struct", TestModuleStaticPerStruct},
        {"Module override and optional", TestModuleOverrideAndOptional},
        {"Module forward reference", TestModuleForwardReference},
        {"Module missing error", TestModuleMissingError},
        {"Set terrain type", TestSetTerrainType},
        {"Hashtable values", TestHashtableValues},
        {"Hashtable handles", TestHashtableHandles},
        {"GameCache values and flush", TestGameCacheValuesAndFlush},
        {"GameCache disk persistence", TestGameCacheDiskPersistence},
        {"GameCache store/restore unit", TestGameCacheStoreRestoreUnit},
        {"Revive hero", TestReviveHero},
        {"Ability level shift", TestAbilityLevelShift},
        {"Select unit and food", TestSelectUnitAndFood},
        {"Destructable natives", TestDestructableNatives},
        {"Campaign and flow natives", TestCampaignAndFlowNatives},
        {"Unit point value", TestUnitPointValue},
        {"Set terrain pathable", TestSetTerrainPathable},
        {"Water deforms and doodad anim", TestWaterDeformsAndDoodadAnim},
        {"Terrain deform registry", TestTerrainDeformRegistry},
#ifndef WITHOUT_STORMLIB
        {"StormLib enumerate war3.mpq and W3X maps", RunStormLibTests},
#endif
    };

    int failed = 0;
    for (const auto& tc : tests) {
        if (!RunTest(tc)) {
            ++failed;
        }
    }

    if (failed > 0) {
        std::cout << failed << " test(s) failed." << std::endl;
        return 1;
    }

    std::cout << "All tests passed." << std::endl;
    return 0;
}
