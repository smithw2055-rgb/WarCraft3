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

// Forward declare simulation types
class CUnit;
class CUnitType;
class CSimulation;
class HandleIdAllocator;
class CAbility;
class CUpgradeData;

class CUnitData {
public:
    CUnitData() = default;
    CUnitData(const CGameplayConstants& gameplayConstants,
              const TxtTable& unitData, const TxtTable& unitAbilities,
              const TxtTable& unitBalance, const TxtTable& unitWeapons,
              const TxtTable& unitUI);

    CUnit* CreateUnit(CSimulation& simulation, int playerIndex,
                      War3ID typeId, float x, float y, float facing,
                      HandleIdAllocator& handleIdAllocator,
                      const std::vector<uint8_t>* buildingPathing = nullptr) const;

    CUnitType* GetUnitType(War3ID typeId) const;
    bool HasUnitType(War3ID typeId) const;

    const std::unordered_map<War3ID, std::unique_ptr<CUnitType>>& GetAllTypes() const { return types_; }

    void AddDefaultAbilitiesToUnit(CSimulation& simulation, HandleIdAllocator& handleIdAllocator,
                                   const CUnitType& unitType, bool addDefault, int manaInitial,
                                   int speed, CUnit& unit) const;
    void ApplyPlayerUpgradesToUnit(CSimulation& simulation, int playerIndex,
                                   const CUnitType& unitType, CUnit& unit) const;

    static CUnitData LoadFromMpq(const MpqManager& mpq, const CGameplayConstants& gameplayConstants);

private:
    CUnitType* ParseUnitType(War3ID id, const TxtTable& unitData, const TxtTable& unitAbilities,
                             const TxtTable& unitBalance, const TxtTable& unitWeapons,
                             const TxtTable& unitUI) const;

    std::unordered_map<War3ID, std::unique_ptr<CUnitType>> types_;
};
