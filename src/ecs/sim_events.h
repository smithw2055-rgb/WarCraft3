#pragma once
#include <cstdint>
#include "core/signal.h"
#include "resource/slk.h"

// Combat floating text kind (MISS / CRIT / BASH)
enum class SimCombatTextKind : uint8_t {
    MISS = 0,
    CRIT = 1,
    BASH = 2,
};

// ============================================================
// SimEventBus - Central multicast event bus for CSimulation.
//
// Decouples deterministic simulation from JASS scripting, UI,
// audio, and visual presentation layers. Multiple systems can
// subscribe concurrently via .Connect(...) without overwriting
// each other.
// ============================================================
struct SimEventBus {
    // Lifecycle signals (Phase 5 Modernization: Signal-driven Presentation)
    // (unitHandle)
    Signal<int> onUnitSpawned;
    // (unitHandle)
    Signal<int> onUnitRemoved;
    // (itemHandle)
    Signal<int> onItemSpawned;

    // (unitHandle, killerHandle, ownerPlayerIndex)
    Signal<int, int, int> onUnitDeath;
    // (unitHandle, sourceHandle, damageAmount)
    Signal<int, int, float> onUnitDamage;
    // (unitHandle, orderId, targetHandle, targetX, targetY)
    Signal<int, int, int, float, float> onUnitOrder;
    // (unitHandle, itemHandle, itemEventType)
    Signal<int, int, int> onItemEvent;
    // (transportHandle, passengerHandle)
    Signal<int, int> onUnitLoaded;
    // (unitHandle)
    Signal<int> onUnitDecay;
    // (buyerHandle, sellerHandle, itemHandle)
    Signal<int, int, int> onShopItemSold;
    // (buyerHandle, sellerHandle, hiredUnitHandle)
    Signal<int, int, int> onShopUnitSold;
    // (buyerShopHandle, sellerUnitHandle, itemHandle)
    Signal<int, int, int> onItemPawned;
    // (unitHandle, skillId, newLevel)
    Signal<int, int, int> onHeroSkillLearned;
    // (attackerHandle, victimHandle, kind, amount)
    Signal<int, int, SimCombatTextKind, float> onCombatText;
    // (buildingHandle, typeId, kind, started)
    Signal<int, int, int, bool> onQueueEvent;
    // (buildingHandle, finished)
    Signal<int, bool> onConstructEvent;
    // (unitHandle, explodeBuffId)
    Signal<int, War3ID> onDeathExplode;
    // (unitHandle, prevOwnerPlayerIndex)
    Signal<int, int> onOwnerChanged;
    // (itemHandle)
    Signal<int> onItemRemoved;
    // (buildingHandle, trainedUnitHandle)
    Signal<int, int> onUnitTrained;
    // (summonerHandle, summonedUnitHandle)
    Signal<int, int> onUnitSummoned;
    // (buildingHandle, upgradeId)
    Signal<int, int> onResearchFinished;
    // (reviverHandle, revivedHeroHandle)
    Signal<int, int> onHeroRevived;
    // (casterHandle, stage, targetWidgetHandle, targetX, targetY)
    Signal<int, int, int, float, float> onSpellEvent;

    // Phase 14 Techtree/Upgrade & Victory/Defeat module signals
    // (playerIndex, techRawcode, newLevel)
    Signal<int, uint32_t, int> onPlayerTechLevelChanged;
    // (playerIndex)
    Signal<int> onPlayerDefeated;
    // (playerIndex)
    Signal<int> onPlayerVictorious;
    // (playerIndex, isCrippled)
    Signal<int, bool> onPlayerCrippled;
    // (victory, winningPlayerIndex)
    Signal<bool, int> onGameEnd;
};
