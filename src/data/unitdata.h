#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include "resource/slk.h"

class MpqManager;
class TxtTable;
class SlkTable;
struct CGameplayConstants;
class CUnitType;

// ============================================================================
// CUnitData — Pure Data-Driven Unit Archetype Table (Unity ScriptableObject /
// Godot Resource Catalog style).
//
// Phase 16 Simplification: Removed obsolete ECS-coupled factory/upgrade stubs
// (`CreateUnit`, `AddDefaultAbilitiesToUnit`, `ApplyPlayerUpgradesToUnit`) so
// the `src/data/` layer has zero dependencies on `CUnit` or `CSimulation`.
// ============================================================================
class CUnitData {
public:
    CUnitData() = default;
    CUnitData(const CGameplayConstants& gameplayConstants,
              const SlkTable& unitDataSlk, const TxtTable& unitDataTxt,
              const SlkTable& unitAbilitiesSlk, const TxtTable& unitAbilitiesTxt,
              const SlkTable& unitBalanceSlk, const TxtTable& unitBalanceTxt,
              const SlkTable& unitWeaponsSlk, const TxtTable& unitWeaponsTxt,
              const SlkTable& unitUISlk, const TxtTable& unitUITxt);

    CUnitType* GetUnitType(War3ID typeId) const;
    bool HasUnitType(War3ID typeId) const;
    // Test and script registration. Replaces an existing row for the same id.
    void RegisterUnitType(War3ID typeId, std::unique_ptr<CUnitType> type);

    const std::unordered_map<War3ID, std::unique_ptr<CUnitType>>& GetAllTypes() const { return types_; }

    static CUnitData LoadFromMpq(const MpqManager& mpq, const CGameplayConstants& gameplayConstants);
    static CUnitData LoadFromMpq(const MpqManager& mpq, const CGameplayConstants& gameplayConstants,
                                 const std::string& mapPath);

private:
    std::unordered_map<War3ID, std::unique_ptr<CUnitType>> types_;
};
