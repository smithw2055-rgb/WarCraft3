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
bool TestMipChainCountsAndDims();
bool TestMipChainEmbeddedFallback();
bool TestBLPLevelsWalkPalettedMips();
bool TestDDSLevelsWalkAndTruncate();
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
bool TestStartGamePackageRoundTrip();
bool TestDesyncTrackerLatchesFirst();
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
bool TestSameCodeAurasMergePerCaster();
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
bool TestArtilleryProjectileSplashes();
bool TestAttackGroundShellsPoint();
bool TestMissileLineSpillsCorridor();
bool TestEvasionNegatesAttacks();
bool TestEvasionSourcesCombine();
bool TestCritSourcesHighestWins();
bool TestCombatTextEventsFire();
bool TestBashFiresCombatText();
bool TestCriticalStrikeMultipliesDamage();
bool TestAbilityAndBuffMapMods();
bool TestUpgradeMapModsAndBaseCosts();
bool TestThornsAuraGrantsReflectBuff();
bool TestVampiricAuraHealsAttacker();
bool TestVampiricAuraViaAuraAbility();
bool TestThornsScalesOnIncomingBase();
bool TestThornsRequiresNormalType();
bool TestThornsBlockedByAlliance();
bool TestThornsResistsGate();
bool TestThornsStrongestWins();
bool TestLeechBlockedByPassiveAlliance();
bool TestLeechResistsGate();
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
bool TestHeroCarryRoundTrip();
bool TestHeroCarryFilters();
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
bool TestDamagedUnitReturnsFire();
bool TestDamagedPeonFlees();
bool TestBusyUnitIgnoresReturnFire();
bool TestNewOrderPreemptsMove();
bool TestStunDefersOrdersUntilExpiry();
bool TestUnitDamageAppliesArmorTypeMultiplier();
bool TestNonNormalDamageTypeIgnoresArmorValue();
bool TestNegativeArmorAmplifiesDamage();
bool TestBonusDamageIsMultipliedNotBypassing();
bool TestCombatSystemBlockedMathConsistent();
bool TestHighlandMissUphill();
bool TestHighlandNoMissDownhillFlatAndSpells();
bool TestHighlandNoMissVsDestructableAndOffMap();
bool TestItemLifeSurvivesHeroRecalc();
bool TestUpgradeLifeSurvivesHeroLevelUp();
bool TestMaxLossClampsLifeButGainDoesNotHeal();
bool TestManaLayersStack();
bool TestNonHeroFallbackStacksWithItemAndUpgrade();
bool TestMiscDataTableAndArmorParsed();
bool TestSpellsRowFallsBackToMagic();
bool TestMalformedRowKeepsDefaults();
bool TestMidiParsesNotes();
bool TestMidiRendersAudibleLoop();
bool TestMidiRetailAmbientRenders();
bool TestConfiguredTableDrivesDamage();
bool TestConfiguredDefenseArmorDrivesReduction();
bool TestUnitClassificationKeys();
bool TestWeaponRangedClassification();
bool TestProjectileHitsAfterTravelTime();
bool TestProjectileFizzlesOnDeadTarget();
bool TestProjectileHitsWithoutAttacker();
bool TestProjectileMaxTravelGuard();
bool TestTechtreeUnlockLifecycle();
bool TestTechtreeFlatRequirementsGate();
bool TestHeroTierGatesSecondHero();
bool TestLoadInRangeIsInstant();
bool TestLoadOutOfRangeWalksThenLoads();
bool TestLoadCapacityEnforced();
bool TestUnloadRestoresUnits();
bool TestCargoFollowsTransport();
bool TestTransportDeathKillsCargo();
bool TestDeathRefundsFoodAtDeath();
bool TestRoarAndInnerFireSum();
bool TestSplashMissHalvesWholeAoE();
bool TestArtilleryMissIsSilentButReduced();
bool TestNonSplashMissZeroes();
bool TestNegativeDamageHealsCapped();
bool TestNegativeDamageViaApplyDamage();
bool TestInvulnerableRunsTakenListeners();
bool TestInvulnerableStillShowsCritText();
bool TestTechCapCountsPossession();
bool TestTechBanBlocksTraining();
bool TestTechBanBlocksResearch();
bool TestApplyW3iAvailability();
bool TestResolveGroupUnitById();
bool TestResolveGroupUnitIndexFallback();
bool TestResolveGroupUnitMisses();
bool TestCollectItemTableDrops();
bool TestDeathDropsOnDeathTick();
bool TestSameAuraTwoCastersTakesMax();
bool TestDifferentAurasSum();
bool TestHasteAddsAcrossKeys();
bool TestItemHasteStacksWithBuffHaste();bool TestDeathPopsOutWorker();
bool TestDeathVisionRevealsOwnerAndKiller();
bool TestConstructingDeathFiresExplode();bool TestLoadFiresUnitLoadedCallback();
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
bool TestJassAbilityObjects();
bool TestJassP1Natives();
bool TestJassTargeting();
bool TestJassBonus();
bool TestJassFiles();
bool TestJassSmallBatch();
bool TestFrameNatives();
bool TestLocalStoreNatives();
bool TestBlzFrameNatives();
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
bool TestAttackModifiersDoNotStack();
bool TestAttackModifierRemovedWithAbility();
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
bool TestBuildFootprintUsesPathTex();
bool TestWpmStampRebuild();
bool TestBuildingStampLifecycle();
bool TestDestructableDeathSwap();
bool TestBlightAndHauntedMine();
bool TestWispAndLumberHarvest();
bool TestAcolyteBindingPersistsAndReleasesOnDeath();
bool TestWispJumpsOntoTreeAndDetachesOnDeath();
bool TestGoldMineCapacityMatchesRetail();
bool TestCargoBurrowAndDrop();
bool TestWaygateAndShops();
bool TestTavernHiresHero();
bool TestHeroTokensAndReviveFormula();
bool TestSacrificeSummonsShade();
bool TestSpells28FactoryCoverage();
bool TestSpells28OrderIds();
bool TestHarvestAnimationCarryState();
bool TestHarvestReturnsCarriedBeforeSwitchingResource();
bool TestLumberHarvestReturnsPartialWhenNoTreesRemain();
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
bool TestTerrainFogNatives();
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
bool TestSaveSlotFileOps();
bool TestTriggerExtraNatives();
bool TestSoundStateNatives();
bool TestGroupEnumCountedNatives();
bool TestTriggerExecuteWait();
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
bool TestMeleeAITrainsHero();
bool TestMeleeAIUpkeepThrottlesArmy();
bool TestMeleeAIExpandsToSecondMine();
bool TestMeleeAIBuildsTowers();
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
bool TestMovementTypePathingBits();
bool TestBoundaryFlagQueries();
bool TestMaplessPathingPasses();
bool TestTargetTypeInfoParses();
bool TestDestructableTargetedAsGate();
bool TestDestructableDeathDrops();
bool TestDestructableOccluderInit();
bool TestRetailTargTypeParses();
bool TestSpellTargetWidgets();
bool TestUnitIllusionFlag();
bool TestEffectZRidesFlyHeight();
bool TestPortraitPathRule();
bool TestAuditFoodCapConstructingBuilding();
bool TestAuditPlayerHeroMaintenance();
bool TestAuditTransportCargoDeathCleanup();
bool TestAuditMoveSpeedDriftAndClamp();
bool TestAuditAOEAlliedFiltering();
bool TestAuditStateFlagsBuffCounter();
bool TestAuditBuffStackReplaceAndStunRefresh();
bool TestAuditMorphLifeManaRatio();
bool TestAuditItemHealthManaRegen();
bool TestAuditFogSightOcclusionAndShared();
bool TestAuditLightningShieldFriendlyDamage();
bool TestAuditCreateItemValidation();
bool TestAuditPropulsionWindowDegrees();
bool TestAuditSequenceGoldLumberFilter();
bool TestSignalAndSimEventBusMulticast();
bool TestJassBinderAutoRegistration();
bool TestBlzFrameLiveUIFrameBridgeAndEvents();
bool TestUnitAttributeAggregatorPipeline();
bool TestDataDrivenSpellDescriptorAndRegistry();
bool TestAbilityFactoryO1TableAndAliases();
bool TestSparseSetEcsRegistryAndUnitComponents();
bool TestJassBytecodeCompilerAndIndexedLocals();
bool TestRenderServerMaterialsAndFrustumCulling();
bool TestRenderServerInstancingAndSimulationInterpolation();
bool TestPhase5TrueEcsComponentPooling();
bool TestPhase5SpatialGridAndSystemPipeline();
bool TestPhase5UnitPresentationSystemSignals();
bool TestPhase5StageASceneRenderServerClosedLoop();
bool TestPhase5StageDUiSceneTreeAndDirtyLayout();
bool TestPhase5StageDDataDrivenSpellConsolidation();
bool TestPhase6StageEResourceServerCachingAndVfsUri();
bool TestPhase6StageEResourceServerAsyncPreloadAndGc();
bool TestPhase6StageFBonePaletteAtlasPacking();
bool TestPhase6StageFHardwareInstancedBatchExecution();
bool TestPhase6StageGJobSystemParallelForAndTaskGraph();
bool TestPhase6StageGParallelSkeletonUpdateAndFrustumCulling();
bool TestPhase6StageHStaticSpatialChunkHierarchyCulling();
bool TestPhase6StageHSceneManagerDoodadChunkAcceleration();
bool TestPhase6StageIAudioServerBusHierarchyAnd3DSpatialization();
bool TestPhase6StageIAudioPolyphonyVoiceStealingAndSimEvents();
bool TestPhase7StageJMpqMultiStageDecompressionAndAudioClip();
bool TestPhase7StageJRetailDriveMapMpqAndAudioVerification();
bool TestPhase7StageKBytecodeLocalArraysAndConstantFolding();
bool TestPhase7StageKBytecodeResumableCoroutineTriggerSleep();
bool TestPhase7StageLHotComponentPoolsAndFleetTelemetry();
bool TestPhase7StageLSimStateSnapshotRollbackAndBinaryWire();
bool TestPhase7StageMZeroAllocPathQueryWorkspaceAndFlowField();
bool TestPhase7StageMParallelPathfindingServerBatchDeterminism();
bool TestPhase7StageNTerrainChunkGridAndParticleRibbonBatcher();
bool TestPhase7StageNMdxSharedMeshRegistryAndDeclarativeRenderGraph();
bool TestPhase7StageOWorldSceneSessionHotMapSwitchAndHeroCarry();
bool TestPhase7StageOUIDataBinderReactiveDirtyOnlySync();
bool TestPhase8StagePJassHandleDeduplicationAndTriggerIndex();
bool TestPhase8StagePTimerEventIndexAndDestroyTriggerCleanup();
bool TestPhase8StageQSpatialAcceleratedSplashAndLineDamage();
bool TestPhase8StageQCrowdSteeringRvoAndFlowFieldBlending();
bool TestPhase8StageRInputActionMapChordsAndSerialization();
bool TestPhase8StageRCommandBufferPipelineBoxFormationAndReplay();
bool TestPhase8StageSFastResyncKeyframePackageAndZlibWire();
bool TestPhase8StageSSnapshotDeltaXorZlibCompression();
bool TestPhase8StageTBlpMipTextureResourceAndSiblingFallback();
bool TestPhase8StageTTextureAtlasPackerBorderExtrusionAndMips();
bool TestPhase8StageUEngineProfilerPercentilesAndChromeTrace();
bool TestPhase8StageURetailW3xEndToEndModernizedPipelineBenchmark();
bool TestPhase9StageV1ZeroAllocProcessorAndSquadFlowFieldMove();
bool TestPhase9StageV2SceneManagerIntegratedRenderGraphAndSharedMeshes();
bool TestPhase9StageV3JassZeroAllocNativeCallsAndFastStructSlots();
bool TestPhase9StageV4AudioSystemIntegratedServerAndRetailDriveMultiMapSuite();
bool TestPhase10StageX1FogSinglePassSynthesisAndDirtyTiles();
bool TestPhase10StageX2FogParallelMultiPlayerAndTemporalSmoothTransition();
bool TestPhase10StageY1MdxAnimatorZeroAllocInPlaceAndCursorCache();
bool TestPhase10StageY2MdxAnimatorCrossFadeAndAnimationLod();
bool TestPhase10StageZ1DataDrivenBuffZeroAllocPoolAndStacking();
bool TestPhase10StageZ2TacticalThreatMapUtilityAiAndRetailDriveSuite();
bool TestPhase11StageAA1ParticleSystemZeroAllocStaging();
bool TestPhase11StageAA2RibbonRingBufferZeroAllocTrail();
bool TestPhase11StageBB1BroadphaseSphereRejectionAndCollisionRaycast();
bool TestPhase11StageBB2RaycastInstancesNonAllocAndScreenRectQuery();
bool TestPhase11StageCC1RichTextLayoutCacheAndO1ColorParser();
bool TestPhase11StageCC2Utf8CodepointDecoderAndCjkMetrics();
bool TestPhase11StageDD1UnifiedScriptEngineHostAndJassAdapter();
bool TestPhase11StageDD2RealScriptHotSwapAndRetailDriveSuite();
bool TestPhase12StageEE1EngineSubsystemRegistryAndPhaseLifecycle();
bool TestPhase12StageEE2FixedTimestepStepperAndInterpolationAlpha();
bool TestPhase12StageFF1EngineKernelPluggableGameModuleAndRetailDriveMap();
bool TestPhase12StageGG1UnifiedSlkMergeAndColdUnitScriptOverridesPool();
bool TestPhase13StageHH1War3GameModuleDecoupledUnitAndAbilityAssembly();
bool TestPhase13StageHH2ModuleScopedAbilityCatalogMountAndUnload();
bool TestPhase13StageII1DynamicModuleLoaderSharedLibraryDlopenAndHotReload();
bool TestPhase13StageII2War3GameModuleRetailDriveMapsEndToEndSuite();
bool TestPhase14StageJJ1War3TechtreeTierEquivalencesAndBuildingMorph();
bool TestPhase14StageJJ2War3UpgradeEffectEngineLiveAndSpawnedUnits();
bool TestPhase14StageKK1War3MeleeAndAlliedVictoryDefeatAndCrippledRules();
bool TestPhase14StageKK2DynamicPluginHotReloadAndCloudRetailMapsPhase14Suite();
bool TestPhase15StageLL1War3CombatMatrixArmorCurveAndEtherealVsPureEngine();
bool TestPhase15StageLL2CustomCombatMatrixHotOverrideAndDynamicPluginCombat();
bool TestPhase15StageMM1War3ModuleTacticalAiLifecycleAndArmorCounterScoring();
bool TestPhase15StageMM2RetailDriveMapsCombatAndTacticalAiEndToEndSuite();
bool TestPhase16StageNN1UnitStateBitfieldPackingAndZeroAbilityHeaderCoupling();
bool TestPhase16StageNN2War3ModuleCombatReactionsAndPassiveProcsVsPureEngine();
bool TestPhase16StageOO1War3ModuleBountyDiceHeroXpSplitItemBonusesAndHeroCarry();
bool TestPhase16StageOO2DynamicPluginVersion16AndRetailDriveMapsEndToEndSuite();
bool TestPhase17StagePP1GenericCAbilityCapabilityContractAndZeroBehaviorHeaderCoupling();
bool TestPhase17StagePP2DeclarativeSpellBasePrimitivesWaveLineAndAreaBuffs();
bool TestPhase17StageQQ1UnifiedItemStatBonusSlotsAndWar3CustomSpellDescriptor();
bool TestPhase17StageQQ2DynamicPluginVersion17AndCloudRetailMapsEndToEndSuite();
bool TestPhase18StageRR1CompactCBuffCAbilityLayoutAndCRTPCloneFidelity();
bool TestPhase18StageRR2UnifiedBuffStateRestorationAndPeriodicAoEHelpers();
bool TestPhase18StageSS1PassiveSpellFlagAreaHealAndBlightedMineVirtualContract();
bool TestPhase18StageSS2CustomAutocastSpellRegistrationAndDynamicPluginV18Suite();
bool TestPhase19StageTT1GenericCAbilityQueueBuildUpgradeAndRallyContracts();
bool TestPhase19StageTT2GenericCAbilityHeroProgressionAndShopStockContracts();
bool TestPhase19StageUU1DecoupledJassNativesHeroInventoryShopAndCooldowns();
bool TestPhase19StageUU2War3GameModuleV19BuildSnapObstructionAndDynamicPluginSuite();
bool TestPhase20StageVV1DecoupledMeleeAIAndCrossAbilityVirtualContracts();
bool TestPhase20StageVV2DeclarativeCAbilityAuraAndSetAuraBuffEngine();
bool TestPhase20StageWW1DirectValueCOrderDispatchAndDeduplicatedAbilityBuilderActions();
bool TestPhase20StageWW2War3GameModuleV20DynamicOrdersCustomAurasAndPluginSuite();
bool TestPhase21StageXX1UnifiedMpqSingleArchiveReaderAndJassScriptResolver();
bool TestPhase21StageXX2LayeredMapObjectModsAndCaseInsensitiveSlkMergeHelpers();
bool TestPhase21StageYY1War3GameModuleV21CustomItemUpgradeAndAbilityDataArchetypes();
bool TestPhase21StageYY2DynamicPluginSOV21AndCloudRetailMapsEndToEndSuite();
bool TestPhase22StageZZ1CompactCBuff8BitStateFlagsAndVirtualCombatModifierContract();
bool TestPhase22StageZZ2CompleteCAbilityVirtualContractZeroCrossAbilityCasts();
bool TestPhase22StageAAA1War3GameModuleV22DynamicBuffFactoryAndJassBuffAttachment();
bool TestPhase22StageAAA2DynamicPluginSOV22AndCloudRetailMapsEndToEndSuite();
bool TestPhase23StageBBB1CompactCProjectileLayoutMakeAttackProjectileAndModuleHooks();
bool TestPhase23StageBBB2CompleteCBehaviorPolymorphicContractAndZeroDynamicCast();
bool TestPhase23StageCCC1War3GameModuleV23CustomProjectilesAndCustomBehaviors();
bool TestPhase23StageCCC2DynamicPluginSOV23AndCloudRetailMapsEndToEndSuite();
bool TestPhase24StageDDD1EngineBehaviorStaticFactoriesAndZeroAbilityBehaviorIncludes();
bool TestPhase24StageDDD2CompactCItemCDestructableBitfieldsAndWorkerNavigation();
bool TestPhase24StageEEE1War3GameModuleV24CustomDropoffsAndBehaviorCategoryOverrides();
bool TestPhase24StageEEE2DynamicPluginSOV24AndCloudRetailMapsEndToEndSuite();
bool TestPhase25StageFFF1BitmaskSharedVisionSightAndModuleVisionHooks();
bool TestPhase25StageFFF2WorldCollisionProximityAndPluggableCellPassability();
bool TestPhase25StageGGG1SafePodProjectileSnapshotAndModuleStateRoundTrip();
bool TestPhase25StageGGG2DynamicPluginSOV25AndCloudRetailMapsEndToEndSuite();
bool TestPhase26StageHHH1ReplayMemoryStreamAndPlayerCommandExecution();
bool TestPhase26StageHHH2TurnManagerHistoryCapLockstepHostFastKeyframeResyncAndLobbySkirmish();
bool TestPhase26StageIII1War3ModuleLockstepValidationAlliedTributeAndStateHash();
bool TestPhase26StageIII2DynamicPluginSOV26AndCloudRetailMapsEndToEndSuite();
bool TestPhase27StageJJJ1ScriptEngineHostBaseEventBusAndHotSwapSubscriptionMigration();
bool TestPhase27StageJJJ2BitPackedPathQueryWorkspaceFlatBfsFlowFieldAndWeightedTerrainCost();
bool TestPhase27StageKKK1MultiShapeFormationsAndRoleAwareFrontlineBacklineSorting();
bool TestPhase27StageKKK2DynamicPluginSOV27AndCloudRetailMapsEndToEndSuite();
bool TestPhase28StageLLL1ZeroAllocRenderInterpolationSwapAndBoneAtlasFastReset();
bool TestPhase28StageLLL2AudioServerPrecomputedBusGainsAndPluggableCombatSoundRouting();
bool TestPhase28StageMMM1UIDataBinderCustomReactivePropertiesAndUpkeepFormatter();
bool TestPhase28StageMMM2DynamicPluginSOV28AndCloudRetailMapsEndToEndSuite();
bool TestPhase29StageNNN1EngineProfilerO1RingBufferAndRenderGraphDynamicSortedPasses();
bool TestPhase29StageNNN2ResourceServerGenericCacheKernelAndVfsPathAliases();
bool TestPhase29StageOOO1War3GameModuleV29AssetPathAliasesAndSoundModelRedirection();
bool TestPhase29StageOOO2DynamicPluginSOV29AndCloudRetailMapsEndToEndSuite();
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
        {"Mip chain counts and dims", TestMipChainCountsAndDims},
        {"Mip chain embedded fallback", TestMipChainEmbeddedFallback},
        {"BLP levels walk paletted mips", TestBLPLevelsWalkPalettedMips},
        {"DDS levels walk and truncate", TestDDSLevelsWalkAndTruncate},
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
        {"Start game package round trip", TestStartGamePackageRoundTrip},
        {"Desync tracker latches first", TestDesyncTrackerLatchesFirst},
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
        {"Same-code auras merge per caster", TestSameCodeAurasMergePerCaster},
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
        {"Artillery projectile splashes", TestArtilleryProjectileSplashes},
        {"Attack ground shells point", TestAttackGroundShellsPoint},
        {"Missile line spills corridor", TestMissileLineSpillsCorridor},
        {"Evasion negates attacks", TestEvasionNegatesAttacks},
        {"Evasion sources combine", TestEvasionSourcesCombine},
        {"Crit sources highest wins", TestCritSourcesHighestWins},
        {"Combat text events fire", TestCombatTextEventsFire},
        {"Bash fires combat text", TestBashFiresCombatText},
        {"Critical strike multiplies damage", TestCriticalStrikeMultipliesDamage},
        {"Ability and buff map mods", TestAbilityAndBuffMapMods},
        {"Upgrade map mods and base costs", TestUpgradeMapModsAndBaseCosts},
        {"Thorns aura grants reflect buff", TestThornsAuraGrantsReflectBuff},
        {"Vampiric aura heals attacker", TestVampiricAuraHealsAttacker},
        {"Vampiric aura via aura ability", TestVampiricAuraViaAuraAbility},
        {"Thorns scales on incoming base", TestThornsScalesOnIncomingBase},
        {"Thorns requires normal type", TestThornsRequiresNormalType},
        {"Thorns blocked by alliance", TestThornsBlockedByAlliance},
        {"Thorns resists gate", TestThornsResistsGate},
        {"Thorns strongest wins", TestThornsStrongestWins},
        {"Leech blocked by passive alliance", TestLeechBlockedByPassiveAlliance},
        {"Leech resists gate", TestLeechResistsGate},
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
        {"Hero carry round trip", TestHeroCarryRoundTrip},
        {"Hero carry filters", TestHeroCarryFilters},
        {"MIDI parses notes", TestMidiParsesNotes},
        {"MIDI renders audible loop", TestMidiRendersAudibleLoop},
        {"MIDI retail ambient renders", TestMidiRetailAmbientRenders},
        {"Techtree unlock lifecycle", TestTechtreeUnlockLifecycle},
        {"Techtree flat requirements gate", TestTechtreeFlatRequirementsGate},
        {"Hero tier gates second hero", TestHeroTierGatesSecondHero},
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
        {"Damaged unit returns fire", TestDamagedUnitReturnsFire},
        {"Damaged peon flees", TestDamagedPeonFlees},
        {"Busy unit ignores return fire", TestBusyUnitIgnoresReturnFire},
        {"New order preempts move", TestNewOrderPreemptsMove},
        {"Stun defers orders until expiry", TestStunDefersOrdersUntilExpiry},
        {"Unit damage applies armor-type multiplier", TestUnitDamageAppliesArmorTypeMultiplier},
        {"Non-normal damage ignores armor value", TestNonNormalDamageTypeIgnoresArmorValue},
        {"Negative armor amplifies damage", TestNegativeArmorAmplifiesDamage},
        {"Bonus damage is multiplied, not bypassing", TestBonusDamageIsMultipliedNotBypassing},
        {"Combat blocked math consistent", TestCombatSystemBlockedMathConsistent},
        {"Highland miss uphill", TestHighlandMissUphill},
        {"Highland no miss downhill/flat/spells", TestHighlandNoMissDownhillFlatAndSpells},
        {"Highland no miss vs destructable/off-map", TestHighlandNoMissVsDestructableAndOffMap},
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
        {"Unit classification keys", TestUnitClassificationKeys},
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
        {"Roar and Inner Fire sum", TestRoarAndInnerFireSum},
        {"Splash miss halves whole AoE", TestSplashMissHalvesWholeAoE},
        {"Artillery miss is silent but reduced", TestArtilleryMissIsSilentButReduced},
        {"Non-splash miss zeroes", TestNonSplashMissZeroes},
        {"Negative damage heals capped", TestNegativeDamageHealsCapped},
        {"Negative damage via ApplyDamage", TestNegativeDamageViaApplyDamage},
        {"Invulnerable runs taken listeners", TestInvulnerableRunsTakenListeners},
        {"Invulnerable still shows crit text", TestInvulnerableStillShowsCritText},
        {"Tech cap counts possession", TestTechCapCountsPossession},
        {"Tech ban blocks training", TestTechBanBlocksTraining},
        {"Tech ban blocks research", TestTechBanBlocksResearch},
        {"Apply w3i availability", TestApplyW3iAvailability},
        {"Resolve group unit by id", TestResolveGroupUnitById},
        {"Resolve group unit index fallback", TestResolveGroupUnitIndexFallback},
        {"Resolve group unit misses", TestResolveGroupUnitMisses},
        {"Collect item table drops", TestCollectItemTableDrops},
        {"Death drops on death tick", TestDeathDropsOnDeathTick},
        {"Same aura two casters takes max", TestSameAuraTwoCastersTakesMax},
        {"Different auras sum", TestDifferentAurasSum},
        {"Haste adds across keys", TestHasteAddsAcrossKeys},
        {"Item haste stacks with buff haste", TestItemHasteStacksWithBuffHaste},
        {"Death refunds food at death", TestDeathRefundsFoodAtDeath},
        {"Death pops out worker", TestDeathPopsOutWorker},
        {"Death vision reveals owner and killer", TestDeathVisionRevealsOwnerAndKiller},
        {"Constructing death fires explode", TestConstructingDeathFiresExplode},
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
        {"JASS ability objects", TestJassAbilityObjects},
        {"JASS P1 natives", TestJassP1Natives},
        {"JASS targeting", TestJassTargeting},
        {"JASS bonus", TestJassBonus},
        {"JASS files", TestJassFiles},
        {"JASS small batch", TestJassSmallBatch},
        {"JASS frames", TestFrameNatives},
        {"JASS localstore", TestLocalStoreNatives},
        {"Blz frame natives", TestBlzFrameNatives},
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
        {"Attack modifiers do not stack", TestAttackModifiersDoNotStack},
        {"Attack modifier removed with ability", TestAttackModifierRemovedWithAbility},
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
        {"Build footprint uses pathTex", TestBuildFootprintUsesPathTex},
        {"WPM stamp rebuild", TestWpmStampRebuild},
        {"Building stamp lifecycle", TestBuildingStampLifecycle},
        {"Destructable death swap", TestDestructableDeathSwap},
        {"Blight and haunted mine", TestBlightAndHauntedMine},
        {"Wisp and lumber harvest", TestWispAndLumberHarvest},
        {"Acolyte binding persists, releases on death", TestAcolyteBindingPersistsAndReleasesOnDeath},
        {"Wisp jumps onto tree, detaches on death", TestWispJumpsOntoTreeAndDetachesOnDeath},
        {"Gold mine capacity matches retail", TestGoldMineCapacityMatchesRetail},
        {"Cargo burrow and drop", TestCargoBurrowAndDrop},
        {"Waygate and shops", TestWaygateAndShops},
        {"Tavern hires hero", TestTavernHiresHero},
        {"Hero tokens and revive formula", TestHeroTokensAndReviveFormula},
        {"Sacrifice summons shade", TestSacrificeSummonsShade},
        {"Spells28 factory coverage", TestSpells28FactoryCoverage},
        {"Spells28 order ids", TestSpells28OrderIds},
        {"Harvest animation carry state", TestHarvestAnimationCarryState},
        {"Harvest returns before resource switch", TestHarvestReturnsCarriedBeforeSwitchingResource},
        {"Lumber returns partial when no trees remain", TestLumberHarvestReturnsPartialWhenNoTreesRemain},
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
        {"Terrain fog natives", TestTerrainFogNatives},
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
        {"Save slot file ops", TestSaveSlotFileOps},
        {"Trigger extra natives", TestTriggerExtraNatives},
        {"Sound state natives", TestSoundStateNatives},
        {"Group enum counted natives", TestGroupEnumCountedNatives},
        {"Trigger execute wait", TestTriggerExecuteWait},
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
        {"Melee AI trains hero", TestMeleeAITrainsHero},
        {"Melee AI upkeep throttles army", TestMeleeAIUpkeepThrottlesArmy},
        {"Melee AI expands to second mine", TestMeleeAIExpandsToSecondMine},
        {"Melee AI builds towers", TestMeleeAIBuildsTowers},
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
        {"Movement-type pathing bits", TestMovementTypePathingBits},
        {"Boundary flag queries", TestBoundaryFlagQueries},
        {"Mapless pathing passes", TestMaplessPathingPasses},
        {"Target-type names parse", TestTargetTypeInfoParses},
        {"Destructable targetedAs gate", TestDestructableTargetedAsGate},
        {"Destructable death drops", TestDestructableDeathDrops},
        {"Destructable occluder init", TestDestructableOccluderInit},
        {"Retail targType parses", TestRetailTargTypeParses},
        {"Spell target widgets", TestSpellTargetWidgets},
        {"Unit illusion flag", TestUnitIllusionFlag},
        {"Effect Z rides fly height", TestEffectZRidesFlyHeight},
        {"Portrait path rule", TestPortraitPathRule},
        {"Audit food cap constructing building", TestAuditFoodCapConstructingBuilding},
        {"Audit player hero maintenance", TestAuditPlayerHeroMaintenance},
        {"Audit transport cargo death cleanup", TestAuditTransportCargoDeathCleanup},
        {"Audit move speed drift and clamp", TestAuditMoveSpeedDriftAndClamp},
        {"Audit aoe allied filtering", TestAuditAOEAlliedFiltering},
        {"Audit state flags buff counter", TestAuditStateFlagsBuffCounter},
        {"Audit buff stack replace and stun refresh", TestAuditBuffStackReplaceAndStunRefresh},
        {"Audit morph life mana ratio", TestAuditMorphLifeManaRatio},
        {"Audit item health mana regen", TestAuditItemHealthManaRegen},
        {"Audit fog sight occlusion and shared", TestAuditFogSightOcclusionAndShared},
        {"Audit lightning shield friendly damage", TestAuditLightningShieldFriendlyDamage},
        {"Audit create item validation", TestAuditCreateItemValidation},
        {"Audit propulsion window degrees", TestAuditPropulsionWindowDegrees},
        {"Audit sequence gold lumber filter", TestAuditSequenceGoldLumberFilter},
        {"Phase1 Signal and SimEventBus multicast", TestSignalAndSimEventBusMulticast},
        {"Phase1 JassBinder template auto registration", TestJassBinderAutoRegistration},
        {"Phase1 BlzFrame live UIFrame bridge and events", TestBlzFrameLiveUIFrameBridgeAndEvents},
        {"Phase2 UnitAttributeAggregator stat and aura pipeline", TestUnitAttributeAggregatorPipeline},
        {"Phase2 Data-driven ABSpellDescriptor and level scaling", TestDataDrivenSpellDescriptorAndRegistry},
        {"Phase2 AbilityFactory O(1) registry and aliases", TestAbilityFactoryO1TableAndAliases},
        {"Phase3 Sparse-Set ECS registry and CUnit components", TestSparseSetEcsRegistryAndUnitComponents},
        {"Phase3 JassVM bytecode compiler and indexed local slots", TestJassBytecodeCompilerAndIndexedLocals},
        {"Phase4 RenderServer materials and frustum culling", TestRenderServerMaterialsAndFrustumCulling},
        {"Phase4 RenderServer GPU instancing and simulation interpolation", TestRenderServerInstancingAndSimulationInterpolation},
        {"Phase5 Stage C True ECS SparseComponentPool storage", TestPhase5TrueEcsComponentPooling},
        {"Phase5 Stage C SpatialHashGrid and SimulationSystemPipeline", TestPhase5SpatialGridAndSystemPipeline},
        {"Phase5 Stage B UnitPresentationSystem and SimEventBus cleanup", TestPhase5UnitPresentationSystemSignals},
        {"Phase5 Stage A SceneManager RenderServer and Interpolation closed-loop", TestPhase5StageASceneRenderServerClosedLoop},
        {"Phase5 Stage D1 UIFrame SceneTree Z-Order and dirty-flag layout", TestPhase5StageDUiSceneTreeAndDirtyLayout},
        {"Phase5 Stage D2 Data-driven ABSpellDescriptor consolidation", TestPhase5StageDDataDrivenSpellConsolidation},
        {"Phase6 Stage E1 ResourceServer VFS URI and RID caching", TestPhase6StageEResourceServerCachingAndVfsUri},
        {"Phase6 Stage E2 ResourceServer async preload and ref-count GC", TestPhase6StageEResourceServerAsyncPreloadAndGc},
        {"Phase6 Stage F1 BonePaletteAtlas 2D RGBA32F packing and sampling", TestPhase6StageFBonePaletteAtlasPacking},
        {"Phase6 Stage F2 Hardware instanced batch execution and bone atlas", TestPhase6StageFHardwareInstancedBatchExecution},
        {"Phase6 Stage G1 JobSystem WorkerThreadPool and ParallelFor", TestPhase6StageGJobSystemParallelForAndTaskGraph},
        {"Phase6 Stage G2 Parallel skeleton update and frustum culling", TestPhase6StageGParallelSkeletonUpdateAndFrustumCulling},
        {"Phase6 Stage H1 StaticSpatialChunkGrid 3-state AABB hierarchy culling", TestPhase6StageHStaticSpatialChunkHierarchyCulling},
        {"Phase6 Stage H2 SceneManager doodad chunk acceleration and visibility sync", TestPhase6StageHSceneManagerDoodadChunkAcceleration},
        {"Phase6 Stage I1 AudioServer bus hierarchy ducking and 3D spatialization", TestPhase6StageIAudioServerBusHierarchyAnd3DSpatialization},
        {"Phase6 Stage I2 AudioServer polyphony voice stealing and SimEventBus bridge", TestPhase6StageIAudioPolyphonyVoiceStealingAndSimEvents},
        {"Phase7 Stage J1 MPQ multi-stage audio decompression and AudioClipResource", TestPhase7StageJMpqMultiStageDecompressionAndAudioClip},
        {"Phase7 Stage J2 Retail Drive W3X map MPQ and WAV verification", TestPhase7StageJRetailDriveMapMpqAndAudioVerification},
        {"Phase7 Stage K1 JASS bytecode local arrays and constant folding", TestPhase7StageKBytecodeLocalArraysAndConstantFolding},
        {"Phase7 Stage K2 JASS bytecode resumable coroutine TriggerSleepAction", TestPhase7StageKBytecodeResumableCoroutineTriggerSleep},
        {"Phase7 Stage L1 ECS hot component pools and fleet telemetry", TestPhase7StageLHotComponentPoolsAndFleetTelemetry},
        {"Phase7 Stage L2 SimStateSnapshot deterministic rollback and binary wire", TestPhase7StageLSimStateSnapshotRollbackAndBinaryWire},
        {"Phase7 Stage M1 Zero-allocation PathQueryWorkspace and FlowFieldGrid", TestPhase7StageMZeroAllocPathQueryWorkspaceAndFlowField},
        {"Phase7 Stage M2 Parallel PathfindingServer batch determinism", TestPhase7StageMParallelPathfindingServerBatchDeterminism},
        {"Phase7 Stage N1 TerrainChunkGrid culling and ParticleRibbonBatcher", TestPhase7StageNTerrainChunkGridAndParticleRibbonBatcher},
        {"Phase7 Stage N2 MdxSharedMeshRegistry and declarative RenderGraph", TestPhase7StageNMdxSharedMeshRegistryAndDeclarativeRenderGraph},
        {"Phase7 Stage O1 WorldSceneSession hot map switch and hero carry", TestPhase7StageOWorldSceneSessionHotMapSwitchAndHeroCarry},
        {"Phase7 Stage O2 UIDataBinder reactive dirty-only HUD sync", TestPhase7StageOUIDataBinderReactiveDirtyOnlySync},
        {"Phase8 Stage P1 JASS handle deduplication and O(1) TriggerEventIndex", TestPhase8StagePJassHandleDeduplicationAndTriggerIndex},
        {"Phase8 Stage P2 Timer event index and DestroyTrigger cleanup", TestPhase8StagePTimerEventIndexAndDestroyTriggerCleanup},
        {"Phase8 Stage Q1 SpatialHashGrid accelerated splash and line damage", TestPhase8StageQSpatialAcceleratedSplashAndLineDamage},
        {"Phase8 Stage Q2 CrowdSteeringSystem RVO and FlowField blending", TestPhase8StageQCrowdSteeringRvoAndFlowFieldBlending},
        {"Phase8 Stage R1 InputActionMap chords conflict check and serialization", TestPhase8StageRInputActionMapChordsAndSerialization},
        {"Phase8 Stage R2 CommandBufferPipeline box formation and ReplayLog", TestPhase8StageRCommandBufferPipelineBoxFormationAndReplay},
        {"Phase8 Stage S1 FastResyncKeyframePackage Zlib wire and tail catch-up", TestPhase8StageSFastResyncKeyframePackageAndZlibWire},
        {"Phase8 Stage S2 SnapshotDeltaPacket XOR+Zlib differential codec", TestPhase8StageSSnapshotDeltaXorZlibCompression},
        {"Phase8 Stage T1 BlpMipTextureResource mip chain and sibling fallback", TestPhase8StageTBlpMipTextureResourceAndSiblingFallback},
        {"Phase8 Stage T2 TextureAtlasPacker 1px border extrusion and mips", TestPhase8StageTTextureAtlasPackerBorderExtrusionAndMips},
        {"Phase8 Stage U1 EngineProfiler percentiles and Chrome Trace JSON", TestPhase8StageUEngineProfilerPercentilesAndChromeTrace},
        {"Phase8 Stage U2 Retail W3X end-to-end modernized pipeline benchmark", TestPhase8StageURetailW3xEndToEndModernizedPipelineBenchmark},
        {"Phase9 Stage V1 Zero-alloc CPathfindingProcessor and Squad FlowField move", TestPhase9StageV1ZeroAllocProcessorAndSquadFlowFieldMove},
        {"Phase9 Stage V2 SceneManager integrated RenderGraph and SharedMeshRegistry", TestPhase9StageV2SceneManagerIntegratedRenderGraphAndSharedMeshes},
        {"Phase9 Stage V3 JassVM zero-alloc native calls and O(1) fast struct slots", TestPhase9StageV3JassZeroAllocNativeCallsAndFastStructSlots},
        {"Phase9 Stage V4 AudioSystem server routing and 12-map retail Drive suite", TestPhase9StageV4AudioSystemIntegratedServerAndRetailDriveMultiMapSuite},
        {"Phase10 Stage X1 Fog single-pass synthesis and dirty tiles", TestPhase10StageX1FogSinglePassSynthesisAndDirtyTiles},
        {"Phase10 Stage X2 Fog parallel multi-player and smooth transition", TestPhase10StageX2FogParallelMultiPlayerAndTemporalSmoothTransition},
        {"Phase10 Stage Y1 MdxAnimator zero-alloc in-place and cursor cache", TestPhase10StageY1MdxAnimatorZeroAllocInPlaceAndCursorCache},
        {"Phase10 Stage Y2 MdxAnimator cross-fade and animation LOD", TestPhase10StageY2MdxAnimatorCrossFadeAndAnimationLod},
        {"Phase10 Stage Z1 DataDrivenBuff zero-alloc pool and stacking", TestPhase10StageZ1DataDrivenBuffZeroAllocPoolAndStacking},
        {"Phase10 Stage Z2 TacticalThreatMap utility AI and retail Drive suite", TestPhase10StageZ2TacticalThreatMapUtilityAiAndRetailDriveSuite},
        {"Phase11 Stage AA1 ParticleSystem zero-alloc staging buffer", TestPhase11StageAA1ParticleSystemZeroAllocStaging},
        {"Phase11 Stage AA2 RibbonRingBuffer zero-alloc trail segments", TestPhase11StageAA2RibbonRingBufferZeroAllocTrail},
        {"Phase11 Stage BB1 Broadphase sphere rejection and collision raycast", TestPhase11StageBB1BroadphaseSphereRejectionAndCollisionRaycast},
        {"Phase11 Stage BB2 RaycastInstancesNonAlloc and screen rect query", TestPhase11StageBB2RaycastInstancesNonAllocAndScreenRectQuery},
        {"Phase11 Stage CC1 RichTextLayoutCache and O(1) color parser", TestPhase11StageCC1RichTextLayoutCacheAndO1ColorParser},
        {"Phase11 Stage CC2 UTF-8 codepoint decoder and CJK metrics", TestPhase11StageCC2Utf8CodepointDecoderAndCjkMetrics},
        {"Phase11 Stage DD1 Unified IScriptEngineHost and JassAdapter", TestPhase11StageDD1UnifiedScriptEngineHostAndJassAdapter},
                {"Phase11 Stage DD2 RealScript hot-swap and retail Drive suite", TestPhase11StageDD2RealScriptHotSwapAndRetailDriveSuite},
        {"Phase12 Stage EE1 EngineSubsystemRegistry phase lifecycle and locator", TestPhase12StageEE1EngineSubsystemRegistryAndPhaseLifecycle},
        {"Phase12 Stage EE2 FixedTimestepStepper and interpolation alpha", TestPhase12StageEE2FixedTimestepStepperAndInterpolationAlpha},
        {"Phase12 Stage FF1 EngineKernel pluggable IGameModule and retail Drive map", TestPhase12StageFF1EngineKernelPluggableGameModuleAndRetailDriveMap},
                {"Phase12 Stage GG1 Unified SlkMerge engine and cold UnitScriptOverrides pool", TestPhase12StageGG1UnifiedSlkMergeAndColdUnitScriptOverridesPool},
        {"Phase13 Stage HH1 War3GameModule decoupled unit and ability assembly", TestPhase13StageHH1War3GameModuleDecoupledUnitAndAbilityAssembly},
        {"Phase13 Stage HH2 Module-scoped ability catalog mount and unload", TestPhase13StageHH2ModuleScopedAbilityCatalogMountAndUnload},
        {"Phase13 Stage II1 DynamicModuleLoader shared library dlopen and hot-reload", TestPhase13StageII1DynamicModuleLoaderSharedLibraryDlopenAndHotReload},
        {"Phase13 Stage II2 War3GameModule retail Drive maps end-to-end suite", TestPhase13StageII2War3GameModuleRetailDriveMapsEndToEndSuite},
        {"Phase14 Stage JJ1 War3 techtree tier equivalences and building morph", TestPhase14StageJJ1War3TechtreeTierEquivalencesAndBuildingMorph},
        {"Phase14 Stage JJ2 War3 upgrade effect engine live and spawned units", TestPhase14StageJJ2War3UpgradeEffectEngineLiveAndSpawnedUnits},
        {"Phase14 Stage KK1 War3 melee and allied victory defeat and crippled rules", TestPhase14StageKK1War3MeleeAndAlliedVictoryDefeatAndCrippledRules},
        {"Phase14 Stage KK2 Dynamic plugin SO and cloud maps Phase14 suite", TestPhase14StageKK2DynamicPluginHotReloadAndCloudRetailMapsPhase14Suite},
        {"Phase15 Stage LL1 War3 combat matrix armor curve and ethereal vs PureEngine", TestPhase15StageLL1War3CombatMatrixArmorCurveAndEtherealVsPureEngine},
        {"Phase15 Stage LL2 Custom combat matrix hot override and dynamic plugin SO", TestPhase15StageLL2CustomCombatMatrixHotOverrideAndDynamicPluginCombat},
        {"Phase15 Stage MM1 War3Module tactical AI lifecycle and armor counter scoring", TestPhase15StageMM1War3ModuleTacticalAiLifecycleAndArmorCounterScoring},
        {"Phase15 Stage MM2 Retail Drive maps combat and tactical AI end-to-end suite", TestPhase15StageMM2RetailDriveMapsCombatAndTacticalAiEndToEndSuite},
        {"Phase16 Stage NN1 CUnit 16-bit state bitmask and zero ability header coupling", TestPhase16StageNN1UnitStateBitfieldPackingAndZeroAbilityHeaderCoupling},
        {"Phase16 Stage NN2 War3Module combat reactions and passive procs vs PureEngine", TestPhase16StageNN2War3ModuleCombatReactionsAndPassiveProcsVsPureEngine},
        {"Phase16 Stage OO1 War3Module bounty dice hero XP item bonuses and hero carry", TestPhase16StageOO1War3ModuleBountyDiceHeroXpSplitItemBonusesAndHeroCarry},
        {"Phase16 Stage OO2 Dynamic plugin SO v16 and retail Drive maps end-to-end suite", TestPhase16StageOO2DynamicPluginVersion16AndRetailDriveMapsEndToEndSuite},
        {"Phase17 Stage PP1 Generic CAbility capability contract and zero behavior header coupling", TestPhase17StagePP1GenericCAbilityCapabilityContractAndZeroBehaviorHeaderCoupling},
        {"Phase17 Stage PP2 Declarative CAbilitySpellBase primitives wave line and area buffs", TestPhase17StagePP2DeclarativeSpellBasePrimitivesWaveLineAndAreaBuffs},
        {"Phase17 Stage QQ1 Unified CAbilityItemStatBonus slots and War3 custom spell descriptor", TestPhase17StageQQ1UnifiedItemStatBonusSlotsAndWar3CustomSpellDescriptor},
        {"Phase17 Stage QQ2 Dynamic plugin SO v17 and cloud retail maps end-to-end suite", TestPhase17StageQQ2DynamicPluginVersion17AndCloudRetailMapsEndToEndSuite},
        {"Phase18 Stage RR1 Compact CBuff CAbility layout and CRTP clone fidelity", TestPhase18StageRR1CompactCBuffCAbilityLayoutAndCRTPCloneFidelity},
        {"Phase18 Stage RR2 Unified CBuff state restoration and periodic AoE helpers", TestPhase18StageRR2UnifiedBuffStateRestorationAndPeriodicAoEHelpers},
        {"Phase18 Stage SS1 Passive spell flag area heal and BlightedMine virtual contract", TestPhase18StageSS1PassiveSpellFlagAreaHealAndBlightedMineVirtualContract},
        {"Phase18 Stage SS2 Custom autocast spell registration and dynamic plugin SO v18", TestPhase18StageSS2CustomAutocastSpellRegistrationAndDynamicPluginV18Suite},
        {"Phase19 Stage TT1 Generic CAbility queue build upgrade and rally contracts", TestPhase19StageTT1GenericCAbilityQueueBuildUpgradeAndRallyContracts},
        {"Phase19 Stage TT2 Generic CAbility hero progression and shop stock contracts", TestPhase19StageTT2GenericCAbilityHeroProgressionAndShopStockContracts},
        {"Phase19 Stage UU1 Decoupled JASS natives hero inventory shop and cooldowns", TestPhase19StageUU1DecoupledJassNativesHeroInventoryShopAndCooldowns},
        {"Phase19 Stage UU2 War3GameModule v19 build snap obstruction and dynamic plugin SO", TestPhase19StageUU2War3GameModuleV19BuildSnapObstructionAndDynamicPluginSuite},
        {"Phase20 Stage VV1 Decoupled MeleeAI and cross-ability virtual contracts", TestPhase20StageVV1DecoupledMeleeAIAndCrossAbilityVirtualContracts},
        {"Phase20 Stage VV2 Declarative CAbilityAura and SetAuraBuff engine", TestPhase20StageVV2DeclarativeCAbilityAuraAndSetAuraBuffEngine},
        {"Phase20 Stage WW1 Direct O(1) COrder dispatch and deduplicated AbilityBuilder", TestPhase20StageWW1DirectValueCOrderDispatchAndDeduplicatedAbilityBuilderActions},
        {"Phase20 Stage WW2 War3GameModule v20 dynamic orders custom auras and plugin SO", TestPhase20StageWW2War3GameModuleV20DynamicOrdersCustomAurasAndPluginSuite},
        {"Phase21 Stage XX1 Unified MpqManager archive reader and JASS script resolver", TestPhase21StageXX1UnifiedMpqSingleArchiveReaderAndJassScriptResolver},
        {"Phase21 Stage XX2 Layered map ObjectMods loader and SlkMerge helpers", TestPhase21StageXX2LayeredMapObjectModsAndCaseInsensitiveSlkMergeHelpers},
        {"Phase21 Stage YY1 War3GameModule v21 custom item upgrade and ability archetypes", TestPhase21StageYY1War3GameModuleV21CustomItemUpgradeAndAbilityDataArchetypes},
        {"Phase21 Stage YY2 Dynamic plugin SO v21 and cloud retail maps end-to-end suite", TestPhase21StageYY2DynamicPluginSOV21AndCloudRetailMapsEndToEndSuite},
        {"Phase22 Stage ZZ1 Compact CBuff 8-bit state flags and virtual combat contract", TestPhase22StageZZ1CompactCBuff8BitStateFlagsAndVirtualCombatModifierContract},
        {"Phase22 Stage ZZ2 Complete CAbility virtual contract zero cross-ability casts", TestPhase22StageZZ2CompleteCAbilityVirtualContractZeroCrossAbilityCasts},
        {"Phase22 Stage AAA1 War3GameModule v22 dynamic buff factory and JASS buff attach", TestPhase22StageAAA1War3GameModuleV22DynamicBuffFactoryAndJassBuffAttachment},
        {"Phase22 Stage AAA2 Dynamic plugin SO v22 and cloud retail maps end-to-end suite", TestPhase22StageAAA2DynamicPluginSOV22AndCloudRetailMapsEndToEndSuite},
        {"Phase23 Stage BBB1 Compact CProjectile layout MakeAttackProjectile and module hooks", TestPhase23StageBBB1CompactCProjectileLayoutMakeAttackProjectileAndModuleHooks},
        {"Phase23 Stage BBB2 Complete CBehavior polymorphic contract and zero dynamic_cast", TestPhase23StageBBB2CompleteCBehaviorPolymorphicContractAndZeroDynamicCast},
        {"Phase23 Stage CCC1 War3GameModule v23 custom projectiles and custom behaviors", TestPhase23StageCCC1War3GameModuleV23CustomProjectilesAndCustomBehaviors},
        {"Phase23 Stage CCC2 Dynamic plugin SO v23 and cloud retail maps end-to-end suite", TestPhase23StageCCC2DynamicPluginSOV23AndCloudRetailMapsEndToEndSuite},
        {"Phase24 Stage DDD1 Engine behavior static factories and zero ability behavior includes", TestPhase24StageDDD1EngineBehaviorStaticFactoriesAndZeroAbilityBehaviorIncludes},
        {"Phase24 Stage DDD2 Compact CItem CDestructable bitfields and worker navigation", TestPhase24StageDDD2CompactCItemCDestructableBitfieldsAndWorkerNavigation},
        {"Phase24 Stage EEE1 War3GameModule v24 custom dropoffs and behavior category overrides", TestPhase24StageEEE1War3GameModuleV24CustomDropoffsAndBehaviorCategoryOverrides},
        {"Phase24 Stage EEE2 Dynamic plugin SO v24 and cloud retail maps end-to-end suite", TestPhase24StageEEE2DynamicPluginSOV24AndCloudRetailMapsEndToEndSuite},
        {"Phase25 Stage FFF1 Bitmask shared-vision sight and module vision hooks", TestPhase25StageFFF1BitmaskSharedVisionSightAndModuleVisionHooks},
        {"Phase25 Stage FFF2 WorldCollision proximity and pluggable cell passability", TestPhase25StageFFF2WorldCollisionProximityAndPluggableCellPassability},
        {"Phase25 Stage GGG1 Safe POD projectile snapshot and module state round-trip", TestPhase25StageGGG1SafePodProjectileSnapshotAndModuleStateRoundTrip},
        {"Phase25 Stage GGG2 Dynamic plugin SO v25 and cloud retail maps end-to-end suite", TestPhase25StageGGG2DynamicPluginSOV25AndCloudRetailMapsEndToEndSuite},
        {"Phase26 Stage HHH1 Replay memory stream and player command execution", TestPhase26StageHHH1ReplayMemoryStreamAndPlayerCommandExecution},
        {"Phase26 Stage HHH2 TurnManager history cap LockstepHost fast keyframe resync and Lobby skirmish", TestPhase26StageHHH2TurnManagerHistoryCapLockstepHostFastKeyframeResyncAndLobbySkirmish},
        {"Phase26 Stage III1 War3GameModule v26 lockstep validation allied tribute and state hash", TestPhase26StageIII1War3ModuleLockstepValidationAlliedTributeAndStateHash},
        {"Phase26 Stage III2 Dynamic plugin SO v26 and cloud retail maps end-to-end suite", TestPhase26StageIII2DynamicPluginSOV26AndCloudRetailMapsEndToEndSuite},
        {"Phase27 Stage JJJ1 ScriptEngineHost base event bus and hot-swap subscription migration", TestPhase27StageJJJ1ScriptEngineHostBaseEventBusAndHotSwapSubscriptionMigration},
        {"Phase27 Stage JJJ2 Bit-packed PathQueryWorkspace flat BFS FlowField and weighted terrain cost", TestPhase27StageJJJ2BitPackedPathQueryWorkspaceFlatBfsFlowFieldAndWeightedTerrainCost},
        {"Phase27 Stage KKK1 Multi-shape formations and role-aware frontline/backline sorting", TestPhase27StageKKK1MultiShapeFormationsAndRoleAwareFrontlineBacklineSorting},
        {"Phase27 Stage KKK2 Dynamic plugin SO v27 and cloud retail maps end-to-end suite", TestPhase27StageKKK2DynamicPluginSOV27AndCloudRetailMapsEndToEndSuite},
        {"Phase28 Stage LLL1 Zero-alloc RenderInterpolationBuffer swap and BonePaletteAtlas fast reset", TestPhase28StageLLL1ZeroAllocRenderInterpolationSwapAndBoneAtlasFastReset},
        {"Phase28 Stage LLL2 AudioServer precomputed bus gains and pluggable combat sound routing", TestPhase28StageLLL2AudioServerPrecomputedBusGainsAndPluggableCombatSoundRouting},
        {"Phase28 Stage MMM1 UIDataBinder custom reactive properties and upkeep formatter", TestPhase28StageMMM1UIDataBinderCustomReactivePropertiesAndUpkeepFormatter},
        {"Phase28 Stage MMM2 Dynamic plugin SO v28 and cloud retail maps end-to-end suite", TestPhase28StageMMM2DynamicPluginSOV28AndCloudRetailMapsEndToEndSuite},
        {"Phase29 Stage NNN1 EngineProfiler O(1) ring buffer and RenderGraph dynamic sorted passes", TestPhase29StageNNN1EngineProfilerO1RingBufferAndRenderGraphDynamicSortedPasses},
        {"Phase29 Stage NNN2 ResourceServer generic cache kernel and VFS path aliases", TestPhase29StageNNN2ResourceServerGenericCacheKernelAndVfsPathAliases},
        {"Phase29 Stage OOO1 War3GameModule v29 asset path aliases and redirection", TestPhase29StageOOO1War3GameModuleV29AssetPathAliasesAndSoundModelRedirection},
        {"Phase29 Stage OOO2 Dynamic plugin SO v29 and cloud retail maps end-to-end suite", TestPhase29StageOOO2DynamicPluginSOV29AndCloudRetailMapsEndToEndSuite},
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
