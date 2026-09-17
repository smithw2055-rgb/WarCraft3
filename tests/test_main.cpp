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
