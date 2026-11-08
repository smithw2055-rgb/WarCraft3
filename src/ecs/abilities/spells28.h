#pragma once
#include "ecs/abilities/ability_spell_base.h"

// Batch 28: militia/hippogryph pairing, blight, harvest variants, cargo,
// waygate, shops, and the gold-mine forms those systems sit on.

class CDestructable;
class CAbilityBlightedGoldMine;

enum class CPairMode {
    MilitiaCall,  // Amic: convert nearby peasants
    Militia,      // Amil: morph to militia while a hall is in range
    Mount,        // Acoa: climb onto a hippogryph
    LoadPartner,  // Acoh: take an archer
    Couple        // Acoi: two partners become one unit
};

// Call to arms, militia, mount, and couple-instant.
class CAbilityPairing : public CAbilitySpellBase {
public:
    CAbilityPairing(int handleId, War3ID code, CPairMode mode);
    void DoEffect(CSimulation& game, CUnit* caster) override;
    void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override;
    // Town hall call-to-arms invokes this on each peasant.
    bool Convert(CSimulation& game, CUnit* self) override;
    bool IsConverted() const { return converted_; }
    void SetConverted(bool v) override { converted_ = v; }
    void SetMilitiaDuration(float seconds) { militiaDuration_ = seconds; }
    float GetMilitiaDuration() const { return militiaDuration_; }
    CPairMode GetMode() const { return mode_; }

protected:
    void ApplyEffectData(const CAbilityDefFull& def, int level) override;

private:
    CUnit* FindPartner(CSimulation& game, CUnit* self) const;
    void MorphIfKnown(CSimulation& game, CUnit* unit, War3ID formId);
    CPairMode mode_;
    War3ID partnerCode_;
    War3ID resultId_;
    float radius_ = 512.0f;
    bool converted_ = false;
    float militiaDuration_ = 0.0f;
};

// Blight (Abli) grows a circle around the bearer.
class CAbilityBlight : public CAbility {
public:
    explicit CAbilityBlight(int handleId);
    void OnTick(CSimulation& game, CUnit* unit) override;
    float GetCurrentRadius() const { return current_; }

private:
    float area_ = 768.0f;
    float expansion_ = 64.0f;
    float interval_ = 1.0f;
    float current_ = 0.0f;
    float accum_ = 0.0f;
    bool creates_ = true;
};

// Haunted gold mine (Abgm). Acolytes register as miners; gold is paid
// to the owner on an interval that shortens as more acolytes stand in.
class CAbilityBlightedGoldMine : public CAbility {
public:
    explicit CAbilityBlightedGoldMine(int handleId);
    void OnTick(CSimulation& game, CUnit* unit) override;
    bool IsResourceMine() const override { return true; }
    bool CanAddMiner() const override { return miners_ < maxMiners_; }
    bool AddMiner() override;
    void RemoveMiner() override;
    int GetGold() const override { return gold_; }
    void SetGold(int v) override { gold_ = v; }
    int GetMinerCount() const { return miners_; }
    bool IsActivelyWorking() const override { return miners_ > 0 && gold_ > 0; }

private:
    int gold_ = 12500;
    int goldPerInterval_ = 10;
    float interval_ = 1.0f;
    int maxMiners_ = 5;
    int miners_ = 0;
    float accum_ = 0.0f;
};

// Acolyte harvest (Aaha): stand at a haunted mine.
class CAbilityAcolyteHarvest : public CAbilitySpellBase {
public:
    explicit CAbilityAcolyteHarvest(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override;
    void OnDeath(CSimulation& game, CUnit* unit) override;
    void OnRemove(CSimulation& game, CUnit* unit) override;
    bool CheckCanTarget(CSimulation& game, CUnit* unit, int orderId, CWidget* target) const override;

    bool IsBound() const { return mineUnitHandle_ >= 0; }
    int GetBoundMineHandle() const { return mineUnitHandle_; }

private:
    void Unbind(CSimulation& game);
    int mineUnitHandle_ = -1;
};

// Wisp harvest (Awha): lumber straight to the player, tree takes damage.
class CAbilityWispHarvest : public CAbilitySpellBase {
public:
    explicit CAbilityWispHarvest(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override;
    void OnTick(CSimulation& game, CUnit* unit) override;

    int GetBoundTreeHandle() const { return treeHandle_; }

private:
    int treeHandle_ = -1;
    int lumberPerInterval_ = 5;
    float damagePerInterval_ = 5.0f;
    float interval_ = 1.0f;
    float accum_ = 0.0f;
};

// Lumber harvest (Ahrl): one chop damages the tree and yields a load.
class CAbilityHarvestLumber : public CAbilitySpellBase {
public:
    explicit CAbilityHarvestLumber(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override;

private:
    float damageToTree_ = 10.0f;
    int capacity_ = 10;
};

// Cargo hold (Acar) and burrow cargo (Abun).
class CAbilityCargoHold : public CAbility {
public:
    CAbilityCargoHold(int handleId, War3ID code, bool burrow, int capacity = 4);
    void OnAdd(CSimulation& game, CUnit* unit) override;
    void OnRemove(CSimulation& game, CUnit* unit) override;
    bool TryLoad(CUnit* holder, CUnit* passenger) override;
    void UnloadAll(CUnit* holder) override;
    int GetCapacity() const { return capacity_; }
    void SetCapacity(int v) { if (v > 0) capacity_ = v; }
    int GetCargoCapacity() const override { return capacity_; }
    void SetCargoCapacity(int v) override { SetCapacity(v); }
    bool IsBurrow() const { return burrow_; }

private:
    bool burrow_;
    int capacity_ = 4;
    int speedSteps_ = 0;
};

// Drop instant (Adri) and stand down (Astd): empty the cargo hold.
class CAbilityDropInstant : public CAbilitySpellBase {
public:
    CAbilityDropInstant(int handleId, War3ID code);
    void DoEffect(CSimulation& game, CUnit* caster) override;
};

// Way gate (Awrp).
class CAbilityWayGate : public CAbility {
public:
    explicit CAbilityWayGate(int handleId);
    bool CheckCanTarget(CSimulation& game, CUnit* unit, int orderId, CWidget* target) const override;
    bool CheckBeforeQueue(CSimulation& game, CUnit* caster, int orderId, CWidget* target) override;
    CBehavior* Begin(CSimulation& game, CUnit* caster, int orderId, CWidget* target) override;
    void SetDestination(CUnit* gate, float x, float y);
    bool TryTeleport(CUnit* gate, CUnit* target) const;
    bool IsEnabled() const { return enabled_; }
    float GetWidth() const { return width_; }
    float GetHeight() const { return height_; }

private:
    bool Inside(const CUnit* gate, const CUnit* target) const;
    float width_ = 256.0f;
    float height_ = 256.0f;
    float destX_ = 0.0f;
    float destY_ = 0.0f;
    bool enabled_ = false;
};

// Shop purchase marker (Apit).
class CAbilityShopPurchase : public CAbility {
public:
    explicit CAbilityShopPurchase(int handleId);
};

// Neutral building (Aneu) and shop sharing (Aall): one shopper per player.
class CAbilityNeutralBuilding : public CAbility {
public:
    static constexpr int kPlayerSlots = 12;

    CAbilityNeutralBuilding(int handleId, War3ID code, bool onlyAllies);
    void OnTick(CSimulation& game, CUnit* unit) override;
    bool CheckCanTarget(CSimulation& game, CUnit* unit, int orderId, CWidget* target) const override;
    CBehavior* Begin(CSimulation& game, CUnit* caster, int orderId, CWidget* target) override;
    CUnit* GetActiveUnit() const;
    CUnit* GetSelectedUnit(int playerIndex) const;
    bool OnlyAllies() const { return onlyAllies_; }

private:
    bool CanSelect(const CSimulation& game, const CUnit* shop, const CUnit* other) const;
    void Rescan(CSimulation& game, CUnit* shop);
    CUnit* shopper_[kPlayerSlots] = {};
    bool HireFromTavern(CSimulation& game, CUnit* tavern, CUnit* hero);
    bool onlyAllies_;
    float radius_ = 400.0f;
    float interval_ = 3.0f;
    float accum_ = 0.0f;
    bool primed_ = false;
    CUnit* selected_[kPlayerSlots] = {};
};
