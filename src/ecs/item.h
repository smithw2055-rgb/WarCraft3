#pragma once
#include "ecs/widget.h"
#include "ecs/item_class.h"
#include "ecs/item_bonuses.h"
#include "resource/slk.h"
#include <vector>
#include <string>

// CItem - world item entity
class CItem : public CWidget {
public:
    CItem(int handleId, War3ID typeId, float x, float y);

    // Position (expose protected CWidget methods)
    void SetPosition(float x, float y) { SetX(x); SetY(y); }

    War3ID GetTypeId() const { return typeId_; }
    War3ID GetDropId() const { return dropId_; }
    void SetDropId(War3ID v) { dropId_ = v; }
    int GetGoldValue() const { return goldValue_; }
    void SetGoldValue(int v) { goldValue_ = v; }
    int GetLumberValue() const { return lumberValue_; }
    void SetLumberValue(int v) { lumberValue_ = v; }

    // Item class
    CItemClass GetItemClass() const { return itemClass_; }
    void SetItemClass(CItemClass v) { itemClass_ = v; }

    // Usage flags
    bool IsHidden() const { return hidden_; }
    void SetHidden(bool v) { hidden_ = v; }
    bool IsPowerup() const { return isPowerup_; }
    void SetPowerup(bool v) { isPowerup_ = v; }
    bool IsPermanent() const { return isPermanent_; }
    void SetPermanent(bool v) { isPermanent_ = v; }
    bool IsCharged() const { return isCharged_; }
    void SetCharged(bool v) { isCharged_ = v; }
    bool IsPerishable() const { return isPerishable_; }
    void SetPerishable(bool v) { isPerishable_ = v; }
    bool IsUseAutomatically() const { return useAutomatically_; }
    void SetUseAutomatically(bool v) { useAutomatically_ = v; }

    // Charges
    int GetCharges() const { return charges_; }
    void SetCharges(int v) { charges_ = v; }
    void UseCharge() { if (charges_ > 0) charges_--; }

    // Drop flags
    bool IsDroppable() const { return droppable_; }
    void SetDroppable(bool v) { droppable_ = v; }
    bool IsPawnable() const { return pawnable_; }
    void SetPawnable(bool v) { pawnable_ = v; }
    bool IsDroppedOnDeath() const { return dropOnDeath_; }
    void SetDroppedOnDeath(bool v) { dropOnDeath_ = v; }

    // Inventory state
    bool IsInInventory() const { return inInventory_; }
    void SetInInventory(bool v) { inInventory_ = v; }
    int GetInventorySlot() const { return static_cast<int>(inventorySlot_); }
    void SetInventorySlot(int v) { inventorySlot_ = static_cast<int8_t>(v); }

    // Abilities granted by this item
    const std::vector<War3ID>& GetAbilityList() const { return abilityList_; }
    void SetAbilityList(const std::vector<War3ID>& ids) { abilityList_ = ids; }
    void AddAbility(War3ID id) { abilityList_.push_back(id); }

    const std::vector<CItemBonus>& GetBonuses() const { return bonuses_; }
    void SetBonuses(std::vector<CItemBonus> bonuses) { bonuses_ = std::move(bonuses); }

    // Owner
    CUnit* GetOwner() const { return owner_; }
    void SetOwner(CUnit* u) { owner_ = u; }

    // Invulnerable
    bool IsInvulnerable() const override { return invulnerable_; }
    void SetInvulnerable(bool v) { invulnerable_ = v; }

    // Widget targeting
    bool CanBeTargetedBy(CSimulation& simulation, CUnit* source,
                         const std::vector<CTargetType>& targetsAllowed) const override;

    // Damage
    float Damage(CSimulation& simulation, CUnit* source,
                 bool isAttack, bool isRanged,
                 CAttackType attackType, CDamageType damageType,
                 const std::string& weaponSoundType,
                 float damage, float bonusDamage = 0) override;

    // Name
    const std::string& GetName() const { return name_; }
    void SetName(const std::string& v) { name_ = v; }
    const std::string& GetDescription() const { return description_; }
    void SetDescription(const std::string& v) { description_ = v; }
    const std::string& GetIcon() const { return icon_; }
    void SetIcon(const std::string& v) { icon_ = v; }

    // Tooltip
    const std::string& GetTooltip() const { return tooltip_; }
    void SetTooltip(const std::string& v) { tooltip_ = v; }

    int GetUserData() const { return userData_; }
    void SetUserData(int v) { userData_ = v; }

private:
    War3ID typeId_;
    War3ID dropId_;
    CItemClass itemClass_ = CItemClass::UNKNOWN;
    int8_t inventorySlot_ = -1;
    // Phase 24 Compact 16-bit ItemStateFlag bitfield: packs 11 boolean flags into 2 bytes
    // beside itemClass_ (1B) + inventorySlot_ (1B), saving 24 bytes per CItem instance.
    uint16_t isPowerup_ : 1;
    uint16_t isPermanent_ : 1;
    uint16_t isCharged_ : 1;
    uint16_t isPerishable_ : 1;
    uint16_t useAutomatically_ : 1;
    uint16_t hidden_ : 1;
    uint16_t invulnerable_ : 1;
    uint16_t droppable_ : 1;
    uint16_t pawnable_ : 1;
    uint16_t dropOnDeath_ : 1;
    uint16_t inInventory_ : 1;
    uint16_t reservedFlags_ : 5;

    int goldValue_ = 0;
    int lumberValue_ = 0;
    int charges_ = 0;
    int userData_ = 0;
    CUnit* owner_ = nullptr;

    // Abilities granted
    std::vector<War3ID> abilityList_;
    std::vector<CItemBonus> bonuses_;

    // Display
    std::string name_;
    std::string description_;
    std::string icon_;
    std::string tooltip_;
};
