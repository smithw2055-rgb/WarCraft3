#pragma once
#include <cstdint>
#include <vector>
#include "resource/slk.h"
#include "ecs/unit_type.h"

class CUnit;

// Death-drop item entry (units.doo dropSets + w3i droppedItemTable)
struct UnitDeathDropItem {
    War3ID itemId;
    int chancePercent = 0;
};

// ============================================================
// Phase 3 ECS Orthogonal Data Components for CUnit Subsystems
//
// Decomposes the monolithic CUnit class into focused, cohesive
// data components that can be stored both in CUnit and in
// SimEcsRegistry SparseComponentPools.
// ============================================================

// 1. Building Construction & Upgrade Progress Component
struct UnitConstructionComponent {
    bool constructing = false;
    bool constructionPaused = false;
    float constructionProgress = 0.0f;
    float upgradeProgress = 0.0f;
    int workerHandleId = -1;
    bool workerInside = false;
    bool constructionConsumesWorker = false;
};

// 2. Transport Cargo & Passenger Component
struct UnitCargoComponent {
    std::vector<CUnit*> cargoUnits;
    bool loadedAboard = false;
    bool attacksDisabled = false;
};

// 3. Alternate-Form Morph Snapshot Component (Metamorphosis, Bear/Raven/Robo forms)
struct UnitMorphComponent {
    bool morphed = false;
    CUnitType* originalType = nullptr;
    War3ID originalTypeId = War3ID();
    int savedSpeed = 0;
    int savedBaseMaxLife = 0;
    int savedBaseMaxMana = 0;
    int savedDefense = 0;
    float savedTurnRate = 0.0f;
    PathingGridMovementType savedMovementType = PathingGridMovementType::FOOT;
    float savedCollisionSize = 0.0f;
    bool savedBuilding = false;
    float savedFlyHeight = 0.0f;
};

// 4. Structure Navigation, Rally, Waygate & Footprint Component
struct UnitStructureNavComponent {
    float rallyX = 0.0f;
    float rallyY = 0.0f;
    int rallyOrderId = 0;
    bool waygateActive = false;
    float waygateX = 0.0f;
    float waygateY = 0.0f;
    int pathingStampId = -1;
};

// 5. Visual Presentation & Tint Component
struct UnitVisualComponent {
    float scale = 1.0f;
    float timeScale = 1.0f;
    int vertexR = 255;
    int vertexG = 255;
    int vertexB = 255;
    int vertexA = 255;
    int playerColor = -1;
    bool useAltIcon = false;
};

// 6. Corpse Decay & Death Drop Table Component
struct UnitCorpseDropComponent {
    int deathTurnTick = 0;
    bool corpse = false;
    bool boneCorpse = false;
    bool decaySuspended = false;
    int deathKillerPlayer = -1;
    std::vector<UnitDeathDropItem> deathDrops;
};
