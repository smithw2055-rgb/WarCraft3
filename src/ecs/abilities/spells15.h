#pragma once
#include "ecs/abilities/ability_spell_base.h"

// Batch 15 (abilities): Phoenix summon with egg rebirth. Retail numbers
// from AbilityData.slk (Cool 180/120, Cost 175/125, UnitID hphx).

// ===== Phoenix (Blood Mage, AHpx) - eternal firebird =====
// The bird stands until killed; death leaves an egg (hpxe, ref UnitData)
// that hatches after the ability duration (DataA if duration is empty,
// otherwise 10s). The cycle chains via CBuffPhoenixRebirth.
class CAbilityPhoenix : public CAbilitySpellBase {
public:
    CAbilityPhoenix(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster) override;
    float GetHatchDelay() const { return hatchDelay_; }
protected:
    void ApplyEffectData(const CAbilityDefFull& def, int level) override;
private:
    War3ID phoenixType_ = War3ID("hphx");
    float hatchDelay_ = 10.0f;
};
