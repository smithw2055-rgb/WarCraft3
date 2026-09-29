#pragma once
#include "ecs/abilities/ability_spell_base.h"
#include "ecs/abilities/spells4.h"

class CAbilityGoldMine;

// Drunken Brawler (ANdb): critical strike plus a dodge chance.
// Still a CAbilityCriticalStrike, so the existing crit alias check holds.
class CAbilityDrunkenBrawler : public CAbilityCriticalStrike {
public:
    explicit CAbilityDrunkenBrawler(int handleId);
    void OnAdd(CSimulation& game, CUnit* unit) override;
    void OnRemove(CSimulation& game, CUnit* unit) override;

private:
    float dodge_ = 0.10f;
};

// Entangle Gold Mine (Aent). Hides and pauses the base mine, then raises
// an egol whose Agl2 overlay spends that mine's gold. The normal order
// starts the replacement under construction; entangleinstant finishes it.
class CAbilityEntangleGoldMine : public CAbilitySpellBase {
public:
    explicit CAbilityEntangleGoldMine(int handleId);
    CBehavior* Begin(CSimulation& game, CUnit* caster, int orderId, CWidget* target) override;
    void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override;
    void OnTick(CSimulation& game, CUnit* unit) override;
    void OnRemove(CSimulation& game, CUnit* unit) override;
    bool CheckCanTarget(CSimulation& game, CUnit* unit, int orderId, CWidget* target) const override;
    CUnit* GetReplacement() const { return replacement_; }
    CUnit* GetOriginal() const { return original_; }
    bool IsConstructing() const { return constructing_; }

private:
    void Release(CSimulation& game, bool killReplacement);
    void AdvanceConstruction(CSimulation& game);
    War3ID resultId_ = War3ID("egol");
    CUnit* original_ = nullptr;
    CUnit* replacement_ = nullptr;
    bool instant_ = true;
    bool constructing_ = false;
};

// Entangled gold mine (Aegm). Pays the owner from the hidden parent mine
// on a cargo slot rotation: a tick pays only when the rotating index
// lands on a loaded worker.
class CAbilityEntangledMine : public CAbility {
public:
    explicit CAbilityEntangledMine(int handleId);
    void OnTick(CSimulation& game, CUnit* unit) override;
    void SetParent(CUnit* parent) { parent_ = parent; }
    CUnit* GetParent() const { return parent_; }
    int GetGoldPerInterval() const { return goldPerInterval_; }

private:
    CUnit* parent_ = nullptr;
    int goldPerInterval_ = 10;
    float interval_ = 1.0f;
    float accum_ = 0.0f;
    int index_ = 0;
};

// Channel test (ANcl). Holds the caster for art duration, then finishes.
class CAbilityChannelTest : public CAbilitySpellBase {
public:
    explicit CAbilityChannelTest(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster) override;
    void OnTick(CSimulation& game, CUnit* unit) override;
    bool IsChanneling() const { return channeling_; }
    float GetRemaining() const { return remaining_; }

private:
    float artDuration_ = 2.0f;
    float remaining_ = 0.0f;
    bool channeling_ = false;
};
