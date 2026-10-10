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
bool TestTgaDecodesViaStb();
bool TestMdlFixtureParses();
bool TestMdlRejectsGarbage();
bool TestMdxTimelineSamplesLinearValues();
bool TestMdxTimelineRejectsShortValues();
bool TestMdxTimelineFallsBackWhenTangentsAreMissing();
bool TestMdxTimelineUsesBezierTangents();
bool TestMdxModelTimelineSamplingIgnoresOtherSequences();
bool TestMdxAnimatedGeosetColorConvertsBgrToRgb();
bool TestMdxParsesLayerTextureIdTimelineAsInteger();
bool TestAttachFindIsCaseInsensitive();
bool TestAttachFindFallbacks();
bool TestAttachWorldPosition();
bool TestAttachMatrixMatchesPosition();
bool TestAttachMatrixFollowsBoneRotation();
bool TestInstanceMatrixIdentity();
bool TestCameraRotateAroundPoint();
bool TestInstanceMatrixRotation();
bool TestInstanceMatrixPerAxisScale();
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
bool TestMdxAnimatorBlendsSequenceChange();
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
bool TestJassArrayGlobals();
bool TestJassThreadArrayGlobals();
bool TestJassSetScalarFromArray();
bool TestPointEffectReportsStoredCoords();
bool TestLocEffectResolvesLocation();
bool TestTargetEffectFollowsUnit();
bool TestTargetEffectFallsBackWhenTargetGone();
bool TestTcpLoopbackCommandExchange();
bool TestTcpAcceptTimesOut();
bool TestLobbyReadyGate();
bool TestLobbySlotValidation();
bool TestTurnBatchWireRoundTrip();
bool TestOrderFromCommandShapes();
bool TestTurnManagerCollectsAndSorts();
bool TestLoopbackLockstepGameStaysInSync();
bool TestDesyncIsDetected();
bool TestHostRunsThreeTurns();
bool TestSilentPeerIdles();
bool TestAcceptRejectsWhenFullOrEmpty();
bool TestLanBindWorks();
bool TestTurnManagerDropUnblocks();
bool TestDisconnectEvictsSeat();
bool TestClaimValidation();
bool TestRejoinCatchesUp();
bool TestRejoinPlayerLogic();
bool TestSeedFromHistoryLogic();
bool TestHostMigrationContinues();
bool TestSpectatorReceivesTurns();
bool TestSpectatorDropOnSendFailure();
bool TestLateSpectatorResync();
bool TestPromoteWatcherPlays();
bool TestPromoteValidation();
bool TestPromoteReclaimsDroppedSeat();
bool TestResyncPackageRoundTrip();
bool TestReplayLandsOnLiveHash();
bool TestHostHistoryGrows();
bool TestItemDamageArmorApplyRemoveSymmetric();
bool TestItemStrengthFlowsThroughHeroAbility();
bool TestUpgradeEffectsApplyAndUnapply();
bool TestSlowAndBloodlustRestoreSpeed();
bool TestVampiricAndUnholyAuraIdsDiffer();
bool TestAuraArmorDamageRegenAttackSpeedSymmetric();
bool TestDivineShieldStackingKeepsInvulnerable();
bool TestOverlappingAurasTakeStrongest();
bool TestAuraDispelStripsBonus();
bool TestRegionEnterLeaveFiresOnMigration();
bool TestRegionEnterFilterBlocks();
bool TestUpkeepGoldTaxLevels();
bool TestRacePrefSetQuery();
bool TestTimerDialogSpeedScalesDecay();
bool TestShopSellItemEvent();
bool TestChangeOwnerEvent();
bool TestSimDrivenChangeOwnerEvent();
bool TestShopHireUnitEvent();
bool TestShopPawnItemEvent();
bool TestTextmacroBasic();
bool TestPlayerAbilityAvailable();
bool TestConstructFinishEvent();
bool TestRescueTransfersOwner();
bool TestConstructCancelEvent();
bool TestTrainStartCancelEvent();
bool TestTrainFinishEvent();
bool TestResearchStartCancelEvent();
bool TestUpgradeStartFinishEvent();
bool TestStripHeroLevel();
bool TestHeroSkillEvent();
bool TestTextmacroNested();
bool TestStaticIfBranches();
bool TestStaticIfSkipsNestedIf();
bool TestStaticIfLibraryForward();
bool TestHookRedirects();
bool TestHookInScope();
bool TestHookUnknownErrors();
bool TestLibraryNeedsOrder();
bool TestLifecycleEventsAndPlayerUnitDualMatch();
bool TestEnumDestructablesInRect();
bool TestDoodadTableLoads();
bool TestDoodadSimWiring();
bool TestWeatherEffectLifecycle();
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
bool TestRootMorphBehaviorHoldsThenCompletes();
bool TestMoveIntoRangeHelper();
bool TestNightElfBuildConsumesWisp();
bool TestHumanBuildPausesWhenWorkerLeaves();
bool TestForGroupSleepDoesNotPoisonVm();
bool TestSeedRngReproducesStreams();
bool TestCombatSequencesDeterministic();
bool TestJassRandomFollowsSeed();
bool TestIssueUnitOrderLogsAndForwards();
bool TestHashStateDeterministic();
bool TestReplayLogRoundTrip();
bool TestGetItemBehaviorPicksUp();
bool TestDropItemBehaviorDrops();
bool TestGiveItemBehaviorTransfers();
bool TestTimedFogModifierExpires();
bool TestExtendedFramesBuild();
bool TestEtherealNegatesPhysical();
bool TestEtherealBonusesMagic();
bool TestEtherealAllySkipsBonus();
bool TestMagicImmuneResists();
bool TestGameplayConstantsEtherealParsed();
bool TestUnitSoundsetResolve();
bool TestUnitTypeSoundDefault();
bool TestEtherealBuffLifecycle();
bool TestMagicImmuneBuffLifecycle();
bool TestEtherealBuffBlocksPhysical();
bool TestAckBankResolve();
bool TestAckBankPlaySafeWithoutAudio();
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
bool TestLoadFiresUnitLoadedCallback();
bool TestLoadedTriggerEvent();
bool TestDecayFiresUnitDecayCallback();
bool TestDecayTriggerEvent();
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
bool TestJassP0Natives();
bool TestJassP1Natives();
bool TestJassTargeting();
bool TestJassBonus();
bool TestJassFiles();
bool TestJassSmallBatch();
bool TestFrameNatives();
bool TestLocalStoreNatives();
bool TestJassEventFiring();
bool TestRetailMapSmoke();
bool TestTimedLifeKillsBearerOnExpiry();
bool TestMassTeleportMovesGroup();
bool TestBlinkClampsRange();
bool TestBlinkSlidesOffBlockedCell();
bool TestShockwaveLineDamage();
bool TestStarfallDamagesArea();
bool TestTranquilityHealsAllies();
bool TestResurrectRevivesInRadius();
bool TestResurrectSkipsBoneCorpse();
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
bool TestItemHealGroupHeals();
bool TestItemTownPortalMovesGroup();
bool TestItemInvulnerableShields();
bool TestItemHealSelfCaps();
bool TestItemManaSelfCaps();
bool TestItemClarityRegens();
bool TestItemManaStealDrains();
bool TestItemProtectionArmors();
bool TestSpells8FactoryCoverage();
bool TestOrbOfFrostChills();
bool TestOrbOfCorruptionStrips();
bool TestOrbOfFireProcs();
bool TestIllusionCopiesTarget();
bool TestLightningShieldWandZaps();
bool TestAnkhReincarnates();
bool TestBeastScrollRallies();
bool TestHealingWardHeals();
bool TestSpells9FactoryCoverage();
bool TestCleavingAttackSplashes();
bool TestSearingArrowsProc();
bool TestEnsnareRoots();
bool TestAncestralSpiritRevives();
bool TestHealingWardUnitHeals();
bool TestUnstableConcoctionBlasts();
bool TestAbolishMagicStripsArea();
bool TestClusterRocketsBlast();
bool TestSpells10FactoryCoverage();
bool TestWindWalkHidesAndBreaks();
bool TestSorceressInvisibility();
bool TestPermanentInvisibility();
bool TestShadowmeldNeedsStillness();
bool TestDustRevealsArea();
bool TestFaerieFireReveals();
bool TestBerserkHastesAndSoftens();
bool TestSummonBearStands();
bool TestBattleRoarRallies();
bool TestSpells11FactoryCoverage();
bool TestDetectorsPierceStealth();
bool TestTrueSightPassive();
bool TestSentryWardDetects();
bool TestFlareRevealsArea();
bool TestBurrowHidesAndRoots();
bool TestCannibalizeEats();
bool TestCarrionBeetlesRise();
bool TestWebGroundsFlyers();
bool TestSpells12FactoryCoverage();
bool TestMetamorphosisMorphs();
bool TestBearFormMorphs();
bool TestRavenFormMorphs();
bool TestRoboGoblinMorphs();
bool TestStoneFormStatues();
bool TestAvatarRages();
bool TestSiphonManaDrains();
bool TestBigBadVoodooShields();
bool TestSpells13FactoryCoverage();
bool TestPocketFactoryBuilds();
bool TestLavaSpawnSplits();
bool TestSoulBurnBurns();
bool TestQuilbeastSummons();
bool TestFrostBreathChills();
bool TestDiseaseCloudSickens();
bool TestPulverizeSlams();
bool TestForkedLightningChains();
bool TestSpells14FactoryCoverage();
bool TestPhoenixRebirths();
bool TestPhoenixHatchDelayFromData();
bool TestSpells15FactoryCoverage();
bool TestDevourSwallows();
bool TestDevourMagicDispels();
bool TestColdArrowsProc();
bool TestMonsoonWaves();
bool TestAbsorbManaPops();
bool TestUnholyFrenzyHastes();
bool TestSpells16FactoryCoverage();
bool TestHowlWeakens();
bool TestBreathOfFireBurns();
bool TestDrunkenHazeMisses();
bool TestManaShieldAbsorbs();
bool TestLocustSwarmDevours();
bool TestCycloneSuspends();
bool TestVengeanceRises();
bool TestSpells17FactoryCoverage();
bool TestWhirlwindSpins();
bool TestImmolationBurnsAndDrains();
bool TestSummonsWithoutDataAreGraceful();
bool TestKaboomDetonates();
bool TestPhoenixFireScorches();
bool TestStarfallCodeIsAesf();
bool TestSpells18FactoryCoverage();
bool TestBanishEtherealizes();
bool TestAntiMagicShellImmunizes();
bool TestSpells19FactoryCoverage();
bool TestSpells19OrderIds();
bool TestPossessionConvertsAndSacrifices();
bool TestPossessionNoSacrificeWithoutConversion();
bool TestStampedeTramplesCorridor();
bool TestSpells20FactoryCoverage();
bool TestSpells20OrderIds();
bool TestStasisTrapTripsAndStuns();
bool TestStasisTrapIgnoresAllies();
bool TestTornadoWandersAndTosses();
bool TestTornadoExpires();
bool TestSpells21FactoryCoverage();
bool TestSpells21OrderIds();
bool TestFingerOfDeathExecutes();
bool TestFingerOfDeathSparesHeroes();
bool TestHowlCripplesEnemiesOnly();
bool TestAliasAnswersOwnOrder();
bool TestSpells22FactoryCoverage();
bool TestSpells22OrderIds();
bool TestChainDispelCleansesBothSides();
bool TestTauntForcesAttack();
bool TestSpells23FactoryCoverage();
bool TestSpells23OrderIds();
bool TestRegenAuraReachesAllies();
bool TestItemPassivesApplyAndRemove();
bool TestSpells24FactoryCoverage();
bool TestSpells24AliasOrders();
bool TestCurseMakesAttacksMiss();
bool TestMoonWellSpendsMana();
bool TestEatTreeFellsAndHeals();
bool TestSacrificeKillsTheOffering();
bool TestHideAndEtherealAndGold();
bool TestItemStatBonusesApply();
bool TestSpells25FactoryCoverage();
bool TestSpells25OrderIds();
bool TestSpellShieldBlocksHostileCast();
bool TestSpellShieldAreaCoversAllies();
bool TestPermanentItemsChangeTheHero();
bool TestMoonstoneSetsTheClock();
bool TestFormToggleAndChaos();
bool TestSpells26FactoryCoverage();
bool TestSpells26OrderIds();
bool TestStatePassivesApply();
bool TestSpellResistAndMixedDefense();
bool TestMagicDefenseToggles();
bool TestMagicLeashStuns();
bool TestReplenishRestoresAllies();
bool TestSpells27FactoryCoverage();
bool TestSpells27OrderIds();
bool TestMilitiaAndMountPairing();
bool TestMilitiaReturnsCargoAndExpires();
bool TestUpgradeAndPowerup();
bool TestItemPickupAppliesBonuses();
bool TestBuildRejectsUnbuildableCell();
bool TestBlightAndHauntedMine();
bool TestWispAndLumberHarvest();
bool TestAcolyteBindingPersistsAndReleasesOnDeath();
bool TestWispJumpsOntoTreeAndDetachesOnDeath();
bool TestGoldMineCapacityMatchesRetail();
bool TestCargoBurrowAndDrop();
bool TestWaygateAndShops();
bool TestSacrificeSummonsShade();
bool TestSpells28FactoryCoverage();
bool TestSpells28OrderIds();
bool TestEntangleGoldMinePaysFromParent();
bool TestEntangleConstruction();
bool TestOverlayedMinePaysParent();
bool TestChannelAndDrunkenBrawler();
bool TestSpells29FactoryCoverage();
bool TestSpells29OrderIds();
bool TestOrderStringTable();
bool TestOrderIdConstantsMatchRetail();
bool TestSpellBaseOrderIds();
bool TestSpellOrderDispatch();
bool TestWrongOrderDoesNotCast();
bool TestSmartDoesNotCastSpells();
bool TestPassiveNeverMatchesOrders();
bool TestSmartPointOrderMovesUnit();
bool TestSmartWidgetFallbackMovesUnit();
bool TestFogUnexploredIsMasked();
bool TestFogUnitSightReveals();
bool TestFogExploredMemory();
bool TestFogUpdateSightDirect();
bool TestFogMaskedOverrideWinsOverSight();
bool TestFogModifierLifecycle();
bool TestFogOverridePrecedence();
bool TestFogRasterize();
bool TestFogVersionBumps();
bool TestFogJassRoundTrip();
bool TestFogEnableGate();
bool TestFogSharedVision();
bool TestUnitShareVisionReveals();
bool TestTerrainDeformLowersHeight();
bool TestStealthVisibilityRules();
bool TestLightningSegmentCount();
bool TestLightningDefaultParams();
bool TestLightningEndpointsAndBounds();
bool TestLightningDeterminism();
bool TestLightningBoltBookkeeping();
bool TestLightningSlkOverride();
bool TestImageBillboardPoseAndCorners();
bool TestAbilityLevelsParse();
bool TestAbilityLevelsApplyOnAdd();
bool TestAbilityLevelsFollowSkillLevel();
bool TestAbilityLevelsFallbackWithoutData();
bool TestAbilityDataFieldsParse();
bool TestAbilityEffectDataFollowsLevel();
bool TestAbilityEffectDataRoundTwo();
bool TestAbilityEffectDataRoundThree();
bool TestAbilityEffectDataRoundFour();
bool TestAbilityEffectDataRoundFive();
bool TestAbilityEffectDataRoundSix();
bool TestPerLevelSummonsScale();
bool TestHexUsesMarkerNotStun();
bool TestSleepWakesOnDamage();
bool TestSilenceBlocksCastOnly();
bool TestLifeDrainTicksHealCaster();
bool TestEffectChainDamageAndHeal();
bool TestBuilderRegistryRoundTrip();
bool TestSimRandIsDeterministic();
bool TestSummonWithoutUnitDataIsGraceful();
bool TestHeroSkillLevelsAndGrants();
bool TestABSingleTargetNukeAndStun();
bool TestABForEachInRangeBlizzardWave();
bool TestABTeleportTarget();
bool TestABIfBranch();
bool TestABMissileStun();
bool TestABDispelArea();
bool TestABKillAndResurrect();
bool TestABCreateUnit();
bool TestStormBoltChainParity();
bool TestHolyLightUndeadBranch();
bool TestWarStompChainParity();
bool TestDeathCoilAllyUndeadHealed();
bool TestBlizzardWavesTotal();
bool TestFrostNovaChainParity();
bool TestFanOfKnivesChainParity();
bool TestRainOfFireChainParity();
bool TestFlameStrikeChainParity();
bool TestDeathAndDecayChainParity();
bool TestShockwaveChainParity();
bool TestImpaleChainParity();
bool TestChainLightningBounceParity();
bool TestHealingWaveBounceParity();
bool TestChannelFinishRunsAtEnd();
bool TestChannelTickRunsEachTick();
bool TestChannelInterruptCancels();
bool TestChannelFizzleOnDeath();
bool TestSpellShieldBlocksDamage();
bool TestSpellShieldAllyBypass();
bool TestMissileDeflected();
bool TestHealIgnoresShield();
bool TestCreateVisionModifier();
bool TestCreateVisionExpires();
bool TestAddDefaultAbilities();
bool TestTextTagAgeAndExpiry();
bool TestTextTagPermanentSurvives();
bool TestTextTagPosUnitSnap();
bool TestCascGracefulFailure();
bool TestCascManagerPlumbing();
bool TestCascLiveIfPresent();
bool TestResurrectChainParity();
bool TestThunderClapChainParity();
bool TestManaBurnChainParity();
bool TestEntanglingRootsChainParity();
bool TestVfsFolderLastAddedWins();
bool TestVfsFolderShadowsArchive();
bool TestVfsLastOpenedArchiveWins();
bool TestW3iParsesPlayersAndForces();
bool TestW3iParsesTechAndRandomTables();
bool TestUnitsDooRetainsDropsAndInventory();
bool TestW3dParsesTerrainDoodads();
bool TestIssueOrderByIdNatives();
bool TestOrderIssuedCallbackFiresForAllKinds();
bool TestIssuedOrderTriggerEvents();
bool TestIssueTargetOrderAcceptsDestructable();
bool TestTerrainQueryNatives();
bool TestRectRegionCellNatives();
bool TestPlayerTeamNatives();
bool TestAbilityArtLookupFromProfiles();
bool TestSpellEffectNatives();
bool TestImageNatives();
bool TestTrackableAndUbersplatNatives();
bool TestTrackableEnterAndHit();
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
bool TestMinimapPingExpiry();
bool TestSelectionAndCooldownNatives();
bool TestEndCinematicEvent();
bool TestPlayerTextMessages();
bool TestDawnDuskSwitch();
bool TestCineFilterTransition();
bool TestLeaderboardNatives();
bool TestMultiboardNatives();
bool TestTimerDialogStyleNatives();
bool TestMeleeAIDiscoversHumanPlan();
bool TestMeleeAIEconomyGrows();
bool TestMeleeAIAttackWave();
bool TestMeleeAINativesWiring();
bool TestMeleeAIOrdersEnterCommandLog();
bool TestMeleeAIDifficultyPacing();
bool TestMeleeAIThinkRate();
bool TestMeleeAIResearchesUpgrades();
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
bool TestInterfaceMethodDispatch();
bool TestDelegateForwards();
bool TestModuleImplementsModule();
bool TestSetTerrainType();
bool TestHashtableValues();
bool TestHashtableHandles();
bool TestGameCacheValuesAndFlush();
bool TestGameCacheDiskPersistence();
bool TestGameCacheStoreRestoreUnit();
bool TestGameCacheSyncStored();
bool TestReviveHero();
bool TestAbilityLevelShift();
bool TestSelectUnitAndFood();
bool TestDestructableNatives();
bool TestCampaignAndFlowNatives();
bool TestUnitPointValue();
bool TestAbilityIdConversion();
bool TestRallyTargetGetters();
bool TestSetTerrainPathable();
bool TestWaterDeformsAndDoodadAnim();
bool TestTerrainDeformRegistry();
#ifndef WITHOUT_STORMLIB
bool RunStormLibTests();
#endif

int main(int argc, char** argv) {
    std::string filter;
    if (argc > 1) filter = argv[1];
    std::vector<TestCase> tests = {
        {"W3E loads minimal valid payload", TestW3ELoadValidMinimal},
        {"W3E rejects invalid magic", TestW3ELoadRejectsBadMagic},
        {"BLP rejects invalid magic", TestDecodeBLPRejectsBadMagic},
        {"BLP decodes minimal paletted texture", TestDecodeBLPDecodesMinimalPaletted},
        {"TGA decodes via stb_image", TestTgaDecodesViaStb},
        {"MDL fixture parses", TestMdlFixtureParses},
        {"MDL rejects garbage", TestMdlRejectsGarbage},
        {"MDX timeline samples linear values", TestMdxTimelineSamplesLinearValues},
        {"MDX timeline rejects short value arrays", TestMdxTimelineRejectsShortValues},
        {"MDX timeline falls back without tangents", TestMdxTimelineFallsBackWhenTangentsAreMissing},
        {"MDX timeline uses Bezier tangents", TestMdxTimelineUsesBezierTangents},
        {"MDX model timeline sampling ignores other sequences", TestMdxModelTimelineSamplingIgnoresOtherSequences},
        {"MDX animated geoset color converts BGR to RGB", TestMdxAnimatedGeosetColorConvertsBgrToRgb},
        {"MDX parses layer texture id timeline as integer", TestMdxParsesLayerTextureIdTimelineAsInteger},
        {"Attach find is case insensitive", TestAttachFindIsCaseInsensitive},
        {"Attach find fallbacks", TestAttachFindFallbacks},
        {"Attach world position", TestAttachWorldPosition},
        {"Attach matrix matches position", TestAttachMatrixMatchesPosition},
        {"Attach matrix follows bone rotation", TestAttachMatrixFollowsBoneRotation},
        {"Instance matrix identity", TestInstanceMatrixIdentity},
        {"Camera rotate around point", TestCameraRotateAroundPoint},
        {"Instance matrix rotation", TestInstanceMatrixRotation},
        {"Instance matrix per-axis scale", TestInstanceMatrixPerAxisScale},
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
        {"MDX animator blends sequence change", TestMdxAnimatorBlendsSequenceChange},
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
        {"JASS array globals round-trip", TestJassArrayGlobals},
        {"JASS thread array round-trip", TestJassThreadArrayGlobals},
        {"JASS set scalar from array element", TestJassSetScalarFromArray},
        {"Point effect reports stored coords", TestPointEffectReportsStoredCoords},
        {"Loc effect resolves location", TestLocEffectResolvesLocation},
        {"Target effect follows unit", TestTargetEffectFollowsUnit},
        {"Target effect falls back when target gone", TestTargetEffectFallsBackWhenTargetGone},
        {"TCP loopback command exchange", TestTcpLoopbackCommandExchange},
        {"TCP accept times out", TestTcpAcceptTimesOut},
        {"Lobby ready gate", TestLobbyReadyGate},
        {"Lobby slot validation", TestLobbySlotValidation},
        {"Turn batch wire round trip", TestTurnBatchWireRoundTrip},
        {"Order from command shapes", TestOrderFromCommandShapes},
        {"Turn manager collects and sorts", TestTurnManagerCollectsAndSorts},
        {"Loopback lockstep game stays in sync", TestLoopbackLockstepGameStaysInSync},
        {"Desync is detected", TestDesyncIsDetected},
        {"Host runs three turns", TestHostRunsThreeTurns},
        {"Silent peer idles", TestSilentPeerIdles},
        {"Accept rejects when full or empty", TestAcceptRejectsWhenFullOrEmpty},
        {"LAN bind works", TestLanBindWorks},
        {"Turn manager drop unblocks", TestTurnManagerDropUnblocks},
        {"Disconnect evicts seat", TestDisconnectEvictsSeat},
        {"Claim validation", TestClaimValidation},
        {"Rejoin catches up", TestRejoinCatchesUp},
        {"Rejoin player logic", TestRejoinPlayerLogic},
        {"Seed from history logic", TestSeedFromHistoryLogic},
        {"Host migration continues", TestHostMigrationContinues},
        {"Spectator receives turns", TestSpectatorReceivesTurns},
        {"Spectator drop on send failure", TestSpectatorDropOnSendFailure},
        {"Late spectator resync", TestLateSpectatorResync},
        {"Promote watcher plays", TestPromoteWatcherPlays},
        {"Promote validation", TestPromoteValidation},
        {"Promote reclaims dropped seat", TestPromoteReclaimsDroppedSeat},
        {"Resync package round trip", TestResyncPackageRoundTrip},
        {"Replay lands on live hash", TestReplayLandsOnLiveHash},
        {"Host history grows", TestHostHistoryGrows},
        {"Item damage/armor apply/remove symmetric", TestItemDamageArmorApplyRemoveSymmetric},
        {"Item strength flows through hero ability", TestItemStrengthFlowsThroughHeroAbility},
        {"Upgrade effects apply and unapply", TestUpgradeEffectsApplyAndUnapply},
        {"Slow and bloodlust restore speed", TestSlowAndBloodlustRestoreSpeed},
        {"Vampiric and unholy aura ids differ", TestVampiricAndUnholyAuraIdsDiffer},
        {"Aura armor/regen/attack-speed symmetric", TestAuraArmorDamageRegenAttackSpeedSymmetric},
        {"Divine shield stacking keeps invulnerable", TestDivineShieldStackingKeepsInvulnerable},
        {"Overlapping auras take strongest", TestOverlappingAurasTakeStrongest},
        {"Aura dispel strips bonus", TestAuraDispelStripsBonus},
        {"Region enter/leave fires on migration", TestRegionEnterLeaveFiresOnMigration},
        {"Region enter filter blocks", TestRegionEnterFilterBlocks},
        {"Upkeep gold tax levels", TestUpkeepGoldTaxLevels},
        {"Race pref set query", TestRacePrefSetQuery},
        {"Timer dialog speed scales decay", TestTimerDialogSpeedScalesDecay},
        {"Shop sell item event", TestShopSellItemEvent},
        {"Change owner event", TestChangeOwnerEvent},
        {"Sim-driven change owner event", TestSimDrivenChangeOwnerEvent},
        {"Shop hire unit event", TestShopHireUnitEvent},
        {"Shop pawn item event", TestShopPawnItemEvent},
        {"Textmacro basic", TestTextmacroBasic},
        {"Player ability available", TestPlayerAbilityAvailable},
        {"Construct finish event", TestConstructFinishEvent},
        {"Rescue transfers owner", TestRescueTransfersOwner},
        {"Construct cancel event", TestConstructCancelEvent},
        {"Train start cancel event", TestTrainStartCancelEvent},
        {"Train finish event", TestTrainFinishEvent},
        {"Research start cancel event", TestResearchStartCancelEvent},
        {"Upgrade start finish event", TestUpgradeStartFinishEvent},
        {"Strip hero level", TestStripHeroLevel},
        {"Hero skill event", TestHeroSkillEvent},
        {"Textmacro nested", TestTextmacroNested},
        {"Static if branches", TestStaticIfBranches},
        {"Static if skips nested if", TestStaticIfSkipsNestedIf},
        {"Static if library forward", TestStaticIfLibraryForward},
        {"Hook redirects", TestHookRedirects},
        {"Hook in scope", TestHookInScope},
        {"Hook unknown errors", TestHookUnknownErrors},
        {"Library needs order", TestLibraryNeedsOrder},
        {"Lifecycle events and player-unit dual match", TestLifecycleEventsAndPlayerUnitDualMatch},
        {"Enum destructables in rect", TestEnumDestructablesInRect},
        {"Doodad table loads", TestDoodadTableLoads},
        {"Doodad sim wiring", TestDoodadSimWiring},
        {"Weather effect lifecycle", TestWeatherEffectLifecycle},
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
        {"Root morph holds then completes", TestRootMorphBehaviorHoldsThenCompletes},
        {"Move-into-range helper", TestMoveIntoRangeHelper},
        {"Night elf build consumes wisp", TestNightElfBuildConsumesWisp},
        {"Human build pauses when worker leaves", TestHumanBuildPausesWhenWorkerLeaves},
        {"ForGroup sleep does not poison VM", TestForGroupSleepDoesNotPoisonVm},
        {"Seed RNG reproduces streams", TestSeedRngReproducesStreams},
        {"Combat sequences deterministic", TestCombatSequencesDeterministic},
        {"JASS random follows seed", TestJassRandomFollowsSeed},
        {"IssueUnitOrder logs and forwards", TestIssueUnitOrderLogsAndForwards},
        {"HashState deterministic", TestHashStateDeterministic},
        {"ReplayLog round trip", TestReplayLogRoundTrip},
        {"GetItem behavior picks up", TestGetItemBehaviorPicksUp},
        {"DropItem behavior drops", TestDropItemBehaviorDrops},
        {"GiveItem behavior transfers", TestGiveItemBehaviorTransfers},
        {"Timed fog modifier expires", TestTimedFogModifierExpires},
        {"Extended FDF frames build", TestExtendedFramesBuild},
        {"Ethereal negates physical", TestEtherealNegatesPhysical},
        {"Ethereal bonuses magic", TestEtherealBonusesMagic},
        {"Ethereal ally skips bonus", TestEtherealAllySkipsBonus},
        {"Magic immune resists", TestMagicImmuneResists},
        {"Gameplay constants ethereal parsed", TestGameplayConstantsEtherealParsed},
        {"Unit soundset resolve", TestUnitSoundsetResolve},
        {"Unit type sound default", TestUnitTypeSoundDefault},
        {"Ethereal buff lifecycle", TestEtherealBuffLifecycle},
        {"Magic immune buff lifecycle", TestMagicImmuneBuffLifecycle},
        {"Ethereal buff blocks physical", TestEtherealBuffBlocksPhysical},
        {"Ack bank resolve", TestAckBankResolve},
        {"Ack bank play safe without audio", TestAckBankPlaySafeWithoutAudio},
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
        {"Load fires unit loaded callback", TestLoadFiresUnitLoadedCallback},
        {"Loaded trigger event", TestLoadedTriggerEvent},
        {"Decay fires unit decay callback", TestDecayFiresUnitDecayCallback},
        {"Decay trigger event", TestDecayTriggerEvent},
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
        {"JASS P0 natives", TestJassP0Natives},
        {"JASS P1 natives", TestJassP1Natives},
        {"JASS targeting", TestJassTargeting},
        {"JASS bonus", TestJassBonus},
        {"JASS files", TestJassFiles},
        {"JASS small batch", TestJassSmallBatch},
        {"JASS frames", TestFrameNatives},
        {"JASS localstore", TestLocalStoreNatives},
        {"JASS event firing", TestJassEventFiring},
        {"Retail map smoke", TestRetailMapSmoke},
        {"Timed life kills bearer on expiry", TestTimedLifeKillsBearerOnExpiry},
        {"Mass teleport moves group", TestMassTeleportMovesGroup},
        {"Blink clamps range", TestBlinkClampsRange},
        {"Blink slides off blocked cell", TestBlinkSlidesOffBlockedCell},
        {"Shockwave line damage", TestShockwaveLineDamage},
        {"Starfall damages area", TestStarfallDamagesArea},
        {"Tranquility heals allies", TestTranquilityHealsAllies},
        {"Resurrect revives in radius", TestResurrectRevivesInRadius},
        {"Resurrect skips bone corpse", TestResurrectSkipsBoneCorpse},
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
        {"Item heal group heals", TestItemHealGroupHeals},
        {"Item town portal moves group", TestItemTownPortalMovesGroup},
        {"Item invulnerable shields", TestItemInvulnerableShields},
        {"Item heal self caps", TestItemHealSelfCaps},
        {"Item mana self caps", TestItemManaSelfCaps},
        {"Item clarity regens", TestItemClarityRegens},
        {"Item mana steal drains", TestItemManaStealDrains},
        {"Item protection armors", TestItemProtectionArmors},
        {"Spells8 factory coverage", TestSpells8FactoryCoverage},
        {"Orb of frost chills", TestOrbOfFrostChills},
        {"Orb of corruption strips", TestOrbOfCorruptionStrips},
        {"Orb of fire procs", TestOrbOfFireProcs},
        {"Illusion copies target", TestIllusionCopiesTarget},
        {"Lightning shield wand zaps", TestLightningShieldWandZaps},
        {"Ankh reincarnates", TestAnkhReincarnates},
        {"Beast scroll rallies", TestBeastScrollRallies},
        {"Healing ward heals", TestHealingWardHeals},
        {"Spells9 factory coverage", TestSpells9FactoryCoverage},
        {"Cleaving attack splashes", TestCleavingAttackSplashes},
        {"Searing arrows proc", TestSearingArrowsProc},
        {"Ensnare roots", TestEnsnareRoots},
        {"Ancestral spirit revives", TestAncestralSpiritRevives},
        {"Healing ward unit heals", TestHealingWardUnitHeals},
        {"Unstable concoction blasts", TestUnstableConcoctionBlasts},
        {"Abolish magic strips area", TestAbolishMagicStripsArea},
        {"Cluster rockets blast", TestClusterRocketsBlast},
        {"Spells10 factory coverage", TestSpells10FactoryCoverage},
        {"Wind walk hides and breaks", TestWindWalkHidesAndBreaks},
        {"Sorceress invisibility", TestSorceressInvisibility},
        {"Permanent invisibility", TestPermanentInvisibility},
        {"Shadowmeld needs stillness", TestShadowmeldNeedsStillness},
        {"Dust reveals area", TestDustRevealsArea},
        {"Faerie fire reveals", TestFaerieFireReveals},
        {"Berserk hastes and softens", TestBerserkHastesAndSoftens},
        {"Summon bear stands", TestSummonBearStands},
        {"Battle roar rallies", TestBattleRoarRallies},
        {"Spells11 factory coverage", TestSpells11FactoryCoverage},
        {"Detectors pierce stealth", TestDetectorsPierceStealth},
        {"True sight passive", TestTrueSightPassive},
        {"Sentry ward detects", TestSentryWardDetects},
        {"Flare reveals area", TestFlareRevealsArea},
        {"Burrow hides and roots", TestBurrowHidesAndRoots},
        {"Cannibalize eats", TestCannibalizeEats},
        {"Carrion beetles rise", TestCarrionBeetlesRise},
        {"Web grounds flyers", TestWebGroundsFlyers},
        {"Spells12 factory coverage", TestSpells12FactoryCoverage},
        {"Metamorphosis morphs", TestMetamorphosisMorphs},
        {"Bear form morphs", TestBearFormMorphs},
        {"Raven form morphs", TestRavenFormMorphs},
        {"Robo-goblin morphs", TestRoboGoblinMorphs},
        {"Stone form statues", TestStoneFormStatues},
        {"Avatar rages", TestAvatarRages},
        {"Siphon mana drains", TestSiphonManaDrains},
        {"Big bad voodoo shields", TestBigBadVoodooShields},
        {"Spells13 factory coverage", TestSpells13FactoryCoverage},
        {"Pocket factory builds", TestPocketFactoryBuilds},
        {"Lava spawn splits", TestLavaSpawnSplits},
        {"Soul burn burns", TestSoulBurnBurns},
        {"Quilbeast summons", TestQuilbeastSummons},
        {"Frost breath chills", TestFrostBreathChills},
        {"Disease cloud sickens", TestDiseaseCloudSickens},
        {"Pulverize slams", TestPulverizeSlams},
        {"Forked lightning chains", TestForkedLightningChains},
        {"Spells14 factory coverage", TestSpells14FactoryCoverage},
        {"Phoenix rebirths", TestPhoenixRebirths},
        {"Phoenix hatch delay from data", TestPhoenixHatchDelayFromData},
        {"Spells15 factory coverage", TestSpells15FactoryCoverage},
        {"Devour swallows", TestDevourSwallows},
        {"Devour magic dispels", TestDevourMagicDispels},
        {"Cold arrows proc", TestColdArrowsProc},
        {"Monsoon waves", TestMonsoonWaves},
        {"Absorb mana pops", TestAbsorbManaPops},
        {"Unholy frenzy hastes", TestUnholyFrenzyHastes},
        {"Spells16 factory coverage", TestSpells16FactoryCoverage},
        {"Howl weakens", TestHowlWeakens},
        {"Breath of fire burns", TestBreathOfFireBurns},
        {"Drunken haze misses", TestDrunkenHazeMisses},
        {"Mana shield absorbs", TestManaShieldAbsorbs},
        {"Locust swarm devours", TestLocustSwarmDevours},
        {"Cyclone suspends", TestCycloneSuspends},
        {"Vengeance rises", TestVengeanceRises},
        {"Spells17 factory coverage", TestSpells17FactoryCoverage},
        {"Whirlwind spins", TestWhirlwindSpins},
        {"Immolation burns and drains", TestImmolationBurnsAndDrains},
        {"Summons without data are graceful", TestSummonsWithoutDataAreGraceful},
        {"Kaboom detonates", TestKaboomDetonates},
        {"Phoenix fire scorches", TestPhoenixFireScorches},
        {"Starfall code is AEsf", TestStarfallCodeIsAesf},
        {"Spells18 factory coverage", TestSpells18FactoryCoverage},
        {"Banish etherealizes", TestBanishEtherealizes},
        {"Anti-magic shell immunizes", TestAntiMagicShellImmunizes},
        {"Spells19 factory coverage", TestSpells19FactoryCoverage},
        {"Spells19 order ids", TestSpells19OrderIds},
        {"Possession converts and sacrifices", TestPossessionConvertsAndSacrifices},
        {"Possession no sacrifice without conversion", TestPossessionNoSacrificeWithoutConversion},
        {"Stampede tramples corridor", TestStampedeTramplesCorridor},
        {"Spells20 factory coverage", TestSpells20FactoryCoverage},
        {"Spells20 order ids", TestSpells20OrderIds},
        {"Stasis trap trips and stuns", TestStasisTrapTripsAndStuns},
        {"Stasis trap ignores allies", TestStasisTrapIgnoresAllies},
        {"Tornado wanders and tosses", TestTornadoWandersAndTosses},
        {"Tornado expires", TestTornadoExpires},
        {"Spells21 factory coverage", TestSpells21FactoryCoverage},
        {"Spells21 order ids", TestSpells21OrderIds},
        {"Finger of Death executes", TestFingerOfDeathExecutes},
        {"Finger of Death spares heroes", TestFingerOfDeathSparesHeroes},
        {"Howl cripples enemies only", TestHowlCripplesEnemiesOnly},
        {"Alias answers own order", TestAliasAnswersOwnOrder},
        {"Spells22 factory coverage", TestSpells22FactoryCoverage},
        {"Spells22 order ids", TestSpells22OrderIds},
        {"Chain dispel cleanses both sides", TestChainDispelCleansesBothSides},
        {"Taunt forces attack", TestTauntForcesAttack},
        {"Spells23 factory coverage", TestSpells23FactoryCoverage},
        {"Spells23 order ids", TestSpells23OrderIds},
        {"Regen aura reaches allies", TestRegenAuraReachesAllies},
        {"Item passives apply and remove", TestItemPassivesApplyAndRemove},
        {"Spells24 factory coverage", TestSpells24FactoryCoverage},
        {"Spells24 alias orders", TestSpells24AliasOrders},
        {"Curse makes attacks miss", TestCurseMakesAttacksMiss},
        {"Moon well spends mana", TestMoonWellSpendsMana},
        {"Eat tree fells and heals", TestEatTreeFellsAndHeals},
        {"Sacrifice kills the offering", TestSacrificeKillsTheOffering},
        {"Hide and ethereal and gold", TestHideAndEtherealAndGold},
        {"Item stat bonuses apply", TestItemStatBonusesApply},
        {"Spells25 factory coverage", TestSpells25FactoryCoverage},
        {"Spells25 order ids", TestSpells25OrderIds},
        {"Spell shield blocks hostile cast", TestSpellShieldBlocksHostileCast},
        {"Spell shield area covers allies", TestSpellShieldAreaCoversAllies},
        {"Permanent items change the hero", TestPermanentItemsChangeTheHero},
        {"Moonstone sets the clock", TestMoonstoneSetsTheClock},
        {"Form toggle and chaos", TestFormToggleAndChaos},
        {"Spells26 factory coverage", TestSpells26FactoryCoverage},
        {"Spells26 order ids", TestSpells26OrderIds},
        {"State passives apply", TestStatePassivesApply},
        {"Spell resist and mixed defense", TestSpellResistAndMixedDefense},
        {"Magic defense toggles", TestMagicDefenseToggles},
        {"Magic leash stuns", TestMagicLeashStuns},
        {"Replenish restores allies", TestReplenishRestoresAllies},
        {"Spells27 factory coverage", TestSpells27FactoryCoverage},
        {"Spells27 order ids", TestSpells27OrderIds},
        {"Militia and mount pairing", TestMilitiaAndMountPairing},
        {"Militia returns cargo and expires", TestMilitiaReturnsCargoAndExpires},
        {"Upgrade and powerup", TestUpgradeAndPowerup},
        {"Item pickup applies bonuses", TestItemPickupAppliesBonuses},
        {"Build rejects unbuildable cell", TestBuildRejectsUnbuildableCell},
        {"Blight and haunted mine", TestBlightAndHauntedMine},
        {"Wisp and lumber harvest", TestWispAndLumberHarvest},
        {"Acolyte binding persists, releases on death", TestAcolyteBindingPersistsAndReleasesOnDeath},
        {"Wisp jumps onto tree, detaches on death", TestWispJumpsOntoTreeAndDetachesOnDeath},
        {"Gold mine capacity matches retail", TestGoldMineCapacityMatchesRetail},
        {"Cargo burrow and drop", TestCargoBurrowAndDrop},
        {"Waygate and shops", TestWaygateAndShops},
        {"Sacrifice summons shade", TestSacrificeSummonsShade},
        {"Spells28 factory coverage", TestSpells28FactoryCoverage},
        {"Spells28 order ids", TestSpells28OrderIds},
        {"Entangle gold mine pays from parent", TestEntangleGoldMinePaysFromParent},
        {"Entangle construction", TestEntangleConstruction},
        {"Overlayed mine pays parent", TestOverlayedMinePaysParent},
        {"Channel and drunken brawler", TestChannelAndDrunkenBrawler},
        {"Spells29 factory coverage", TestSpells29FactoryCoverage},
        {"Spells29 order ids", TestSpells29OrderIds},
        {"Order string table", TestOrderStringTable},
        {"Order id constants match retail", TestOrderIdConstantsMatchRetail},
        {"Spell base order ids", TestSpellBaseOrderIds},
        {"Spell order dispatch", TestSpellOrderDispatch},
        {"Wrong order does not cast", TestWrongOrderDoesNotCast},
        {"SMART does not cast spells", TestSmartDoesNotCastSpells},
        {"Passive never matches orders", TestPassiveNeverMatchesOrders},
        {"SMART point order moves unit", TestSmartPointOrderMovesUnit},
        {"SMART widget fallback moves unit", TestSmartWidgetFallbackMovesUnit},
        {"Fog unexplored is masked", TestFogUnexploredIsMasked},
        {"Fog unit sight reveals", TestFogUnitSightReveals},
        {"Fog explored memory", TestFogExploredMemory},
        {"Fog update sight direct", TestFogUpdateSightDirect},
        {"Fog masked override wins over sight", TestFogMaskedOverrideWinsOverSight},
        {"Fog modifier lifecycle", TestFogModifierLifecycle},
        {"Fog override precedence", TestFogOverridePrecedence},
        {"Fog rasterize", TestFogRasterize},
        {"Fog version bumps", TestFogVersionBumps},
        {"Fog JASS round trip", TestFogJassRoundTrip},
        {"Fog enable gate", TestFogEnableGate},
        {"Fog shared vision", TestFogSharedVision},
        {"Unit share vision reveals", TestUnitShareVisionReveals},
        {"Terrain deform lowers height", TestTerrainDeformLowersHeight},
        {"Stealth visibility rules", TestStealthVisibilityRules},
        {"Lightning segment count", TestLightningSegmentCount},
        {"Lightning default params", TestLightningDefaultParams},
        {"Lightning endpoints and bounds", TestLightningEndpointsAndBounds},
        {"Lightning determinism", TestLightningDeterminism},
        {"Lightning bolt bookkeeping", TestLightningBoltBookkeeping},
        {"Lightning SLK override", TestLightningSlkOverride},
        {"Image billboard pose and corners", TestImageBillboardPoseAndCorners},
        {"Ability levels parse", TestAbilityLevelsParse},
        {"Ability levels apply on add", TestAbilityLevelsApplyOnAdd},
        {"Ability levels follow skill level", TestAbilityLevelsFollowSkillLevel},
        {"Ability levels fallback without data", TestAbilityLevelsFallbackWithoutData},
        {"Ability data fields parse", TestAbilityDataFieldsParse},
        {"Ability effect data follows level", TestAbilityEffectDataFollowsLevel},
        {"Ability effect data round two", TestAbilityEffectDataRoundTwo},
        {"Ability effect data round three", TestAbilityEffectDataRoundThree},
        {"Ability effect data round four", TestAbilityEffectDataRoundFour},
        {"Ability effect data round five", TestAbilityEffectDataRoundFive},
        {"Ability effect data round six", TestAbilityEffectDataRoundSix},
        {"Per-level summons scale", TestPerLevelSummonsScale},
        {"Hex uses marker not stun", TestHexUsesMarkerNotStun},
        {"Sleep wakes on damage", TestSleepWakesOnDamage},
        {"Silence blocks cast only", TestSilenceBlocksCastOnly},
        {"Life drain ticks heal caster", TestLifeDrainTicksHealCaster},
        {"Effect chain damage and heal", TestEffectChainDamageAndHeal},
        {"Builder registry round trip", TestBuilderRegistryRoundTrip},
        {"Sim rand is deterministic", TestSimRandIsDeterministic},
        {"Summon without unit data is graceful", TestSummonWithoutUnitDataIsGraceful},
        {"Hero skill levels and grants", TestHeroSkillLevelsAndGrants},
        {"AB single-target nuke and stun", TestABSingleTargetNukeAndStun},
        {"AB foreach-in-range blizzard wave", TestABForEachInRangeBlizzardWave},
        {"AB teleport target", TestABTeleportTarget},
        {"AB if branch", TestABIfBranch},
        {"AB missile stun", TestABMissileStun},
        {"AB dispel area", TestABDispelArea},
        {"AB kill and resurrect", TestABKillAndResurrect},
        {"AB create unit", TestABCreateUnit},
        {"StormBolt chain parity", TestStormBoltChainParity},
        {"HolyLight undead branch", TestHolyLightUndeadBranch},
        {"WarStomp chain parity", TestWarStompChainParity},
        {"DeathCoil ally undead healed", TestDeathCoilAllyUndeadHealed},
        {"Blizzard waves total", TestBlizzardWavesTotal},
        {"FrostNova chain parity", TestFrostNovaChainParity},
        {"FanOfKnives chain parity", TestFanOfKnivesChainParity},
        {"RainOfFire chain parity", TestRainOfFireChainParity},
        {"FlameStrike chain parity", TestFlameStrikeChainParity},
        {"DeathAndDecay chain parity", TestDeathAndDecayChainParity},
        {"Shockwave chain parity", TestShockwaveChainParity},
        {"Impale chain parity", TestImpaleChainParity},
        {"ChainLightning bounce parity", TestChainLightningBounceParity},
        {"HealingWave bounce parity", TestHealingWaveBounceParity},
        {"Channel finish runs at end", TestChannelFinishRunsAtEnd},
        {"Channel tick runs each tick", TestChannelTickRunsEachTick},
        {"Channel interrupt cancels", TestChannelInterruptCancels},
        {"Channel fizzle on death", TestChannelFizzleOnDeath},
        {"Spell shield blocks damage", TestSpellShieldBlocksDamage},
        {"Spell shield ally bypass", TestSpellShieldAllyBypass},
        {"Missile deflected", TestMissileDeflected},
        {"Heal ignores shield", TestHealIgnoresShield},
        {"Create vision modifier", TestCreateVisionModifier},
        {"Create vision expires", TestCreateVisionExpires},
        {"Add default abilities", TestAddDefaultAbilities},
        {"TextTag age and expiry", TestTextTagAgeAndExpiry},
        {"TextTag permanent survives", TestTextTagPermanentSurvives},
        {"TextTag PosUnit snap", TestTextTagPosUnitSnap},
        {"CASC graceful failure", TestCascGracefulFailure},
        {"CASC manager plumbing", TestCascManagerPlumbing},
        {"CASC live if present", TestCascLiveIfPresent},
        {"Resurrect chain parity", TestResurrectChainParity},
        {"ThunderClap chain parity", TestThunderClapChainParity},
        {"ManaBurn chain parity", TestManaBurnChainParity},
        {"EntanglingRoots chain parity", TestEntanglingRootsChainParity},
        {"VFS folder last-added wins", TestVfsFolderLastAddedWins},
        {"VFS folder shadows archive", TestVfsFolderShadowsArchive},
        {"VFS game archive order and map mount", TestVfsLastOpenedArchiveWins},
        {"W3I parses players and forces", TestW3iParsesPlayersAndForces},
        {"W3I parses tech and random tables", TestW3iParsesTechAndRandomTables},
        {"Units.doo retains drops and inventory", TestUnitsDooRetainsDropsAndInventory},
        {"W3D parses terrain doodads", TestW3dParsesTerrainDoodads},
        {"Issue order-by-id natives", TestIssueOrderByIdNatives},
        {"Order issued callback fires for all kinds", TestOrderIssuedCallbackFiresForAllKinds},
        {"Issued order trigger events", TestIssuedOrderTriggerEvents},
        {"Issue target order accepts destructable", TestIssueTargetOrderAcceptsDestructable},
        {"Terrain query natives", TestTerrainQueryNatives},
        {"Rect/region cell natives", TestRectRegionCellNatives},
        {"Player/team natives", TestPlayerTeamNatives},
        {"Ability art lookup from profiles", TestAbilityArtLookupFromProfiles},
        {"Spell effect natives", TestSpellEffectNatives},
        {"Image natives", TestImageNatives},
        {"Trackable and ubersplat natives", TestTrackableAndUbersplatNatives},
        {"Trackable enter and hit", TestTrackableEnterAndHit},
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
        {"Minimap ping expiry", TestMinimapPingExpiry},
        {"Selection and cooldown natives", TestSelectionAndCooldownNatives},
        {"End cinematic event", TestEndCinematicEvent},
        {"Player text messages", TestPlayerTextMessages},
        {"Dawn dusk switch", TestDawnDuskSwitch},
        {"Cine filter transition", TestCineFilterTransition},
        {"Leaderboard natives", TestLeaderboardNatives},
        {"Multiboard natives", TestMultiboardNatives},
        {"Timer dialog style natives", TestTimerDialogStyleNatives},
        {"Melee AI discovers human plan", TestMeleeAIDiscoversHumanPlan},
        {"Melee AI economy grows", TestMeleeAIEconomyGrows},
        {"Melee AI attack wave", TestMeleeAIAttackWave},
        {"Melee AI natives wiring", TestMeleeAINativesWiring},
        {"Melee AI orders enter command log", TestMeleeAIOrdersEnterCommandLog},
        {"Melee AI difficulty pacing", TestMeleeAIDifficultyPacing},
        {"Melee AI think rate", TestMeleeAIThinkRate},
        {"Melee AI researches upgrades", TestMeleeAIResearchesUpgrades},
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
        {"Interface method dispatch", TestInterfaceMethodDispatch},
        {"Delegate forwards", TestDelegateForwards},
        {"Module implements module", TestModuleImplementsModule},
        {"Set terrain type", TestSetTerrainType},
        {"Hashtable values", TestHashtableValues},
        {"Hashtable handles", TestHashtableHandles},
        {"GameCache values and flush", TestGameCacheValuesAndFlush},
        {"GameCache disk persistence", TestGameCacheDiskPersistence},
        {"GameCache store/restore unit", TestGameCacheStoreRestoreUnit},
        {"GameCache sync stored natives", TestGameCacheSyncStored},
        {"Revive hero", TestReviveHero},
        {"Ability level shift", TestAbilityLevelShift},
        {"Select unit and food", TestSelectUnitAndFood},
        {"Destructable natives", TestDestructableNatives},
        {"Campaign and flow natives", TestCampaignAndFlowNatives},
        {"Unit point value", TestUnitPointValue},
        {"Ability id conversion", TestAbilityIdConversion},
        {"Rally target getters", TestRallyTargetGetters},
        {"Set terrain pathable", TestSetTerrainPathable},
        {"Water deforms and doodad anim", TestWaterDeformsAndDoodadAnim},
        {"Terrain deform registry", TestTerrainDeformRegistry},
#ifndef WITHOUT_STORMLIB
        {"StormLib enumerate war3.mpq and W3X maps", RunStormLibTests},
#endif
    };

    int failed = 0;
    for (const auto& tc : tests) {
        if (!filter.empty() && std::string(tc.name).find(filter) == std::string::npos) {
            continue;
        }
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
