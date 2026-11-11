#pragma once
// ============================================================================
// Engine UI: HudViewModel & HudViewBinder — Game-Agnostic MVVM HUD Pipeline
// ============================================================================
#include <algorithm>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace engine::ui {

enum class HudDirtyFlags : uint32_t {
    None         = 0u,
    Resources    = 1u << 0,
    Upkeep       = 1u << 1,
    Selection    = 1u << 2,
    Vitals       = 1u << 3,
    CommandCard  = 1u << 4,
    CustomProps  = 1u << 5,
    All          = 0x3Fu
};

inline constexpr uint32_t HudFlagBits(HudDirtyFlags f) noexcept {
    return static_cast<uint32_t>(f);
}

struct HudCommandSlotView {
    int slotIndex = 0; // 0..11 in 4x3 grid
    uint32_t commandTag = 0;
    std::string iconPath;
    std::string tooltipTitle;
    bool visible = false;
    bool enabled = true;
    bool autocastActive = false;
    float cooldownRatio = 0.0f;

    bool operator==(const HudCommandSlotView& o) const {
        return slotIndex == o.slotIndex &&
               commandTag == o.commandTag &&
               iconPath == o.iconPath &&
               tooltipTitle == o.tooltipTitle &&
               visible == o.visible &&
               enabled == o.enabled &&
               autocastActive == o.autocastActive &&
               cooldownRatio == o.cooldownRatio;
    }
    bool operator!=(const HudCommandSlotView& o) const { return !(*this == o); }
};

class HudViewModel {
public:
    void SetPrimaryResource(int amount) {
        if (primaryResource_ != amount) {
            primaryResource_ = amount;
            dirtyMask_ |= HudFlagBits(HudDirtyFlags::Resources);
        }
    }

    void SetSecondaryResource(int amount) {
        if (secondaryResource_ != amount) {
            secondaryResource_ = amount;
            dirtyMask_ |= HudFlagBits(HudDirtyFlags::Resources);
        }
    }

    void SetSupply(int used, int cap) {
        if (supplyUsed_ != used || supplyCap_ != cap) {
            supplyUsed_ = used;
            supplyCap_ = cap;
            dirtyMask_ |= HudFlagBits(HudDirtyFlags::Resources);
        }
    }

    void SetUpkeep(int level, const std::string& label) {
        if (upkeepLevel_ != level || upkeepLabel_ != label) {
            upkeepLevel_ = level;
            upkeepLabel_ = label;
            dirtyMask_ |= HudFlagBits(HudDirtyFlags::Upkeep);
        }
    }

    void SetSelectedEntity(int handle, const std::string& displayName) {
        if (selectedHandle_ != handle || selectedName_ != displayName) {
            selectedHandle_ = handle;
            selectedName_ = displayName;
            dirtyMask_ |= HudFlagBits(HudDirtyFlags::Selection);
        }
    }

    void SetSelectedVitals(int hp, int maxHp, int energy, int maxEnergy) {
        if (hp_ != hp || maxHp_ != maxHp || energy_ != energy || maxEnergy_ != maxEnergy) {
            hp_ = hp;
            maxHp_ = maxHp;
            energy_ = energy;
            maxEnergy_ = maxEnergy;
            dirtyMask_ |= HudFlagBits(HudDirtyFlags::Vitals);
        }
    }

    void SetCommandSlot(int slotIndex, const HudCommandSlotView& slot) {
        if (slotIndex < 0 || slotIndex >= 12) return;
        if (commandSlots_[static_cast<size_t>(slotIndex)] != slot) {
            commandSlots_[static_cast<size_t>(slotIndex)] = slot;
            dirtyMask_ |= HudFlagBits(HudDirtyFlags::CommandCard);
        }
    }

    void SetCustomProperty(const std::string& key, const std::string& value) {
        auto it = customProps_.find(key);
        if (it == customProps_.end() || it->second != value) {
            customProps_[key] = value;
            dirtyMask_ |= HudFlagBits(HudDirtyFlags::CustomProps);
        }
    }

    void MarkAllDirty() { dirtyMask_ = HudFlagBits(HudDirtyFlags::All); }
    uint32_t ConsumeDirtyMask() {
        const uint32_t m = dirtyMask_;
        dirtyMask_ = 0u;
        return m;
    }
    uint32_t PeekDirtyMask() const { return dirtyMask_; }

    int PrimaryResource() const { return primaryResource_; }
    int SecondaryResource() const { return secondaryResource_; }
    int SupplyUsed() const { return supplyUsed_; }
    int SupplyCap() const { return supplyCap_; }
    int UpkeepLevel() const { return upkeepLevel_; }
    const std::string& UpkeepLabel() const { return upkeepLabel_; }
    int SelectedHandle() const { return selectedHandle_; }
    const std::string& SelectedName() const { return selectedName_; }
    int Hp() const { return hp_; }
    int MaxHp() const { return maxHp_; }
    int Energy() const { return energy_; }
    int MaxEnergy() const { return maxEnergy_; }
    const HudCommandSlotView& CommandSlot(int idx) const {
        return commandSlots_[static_cast<size_t>(std::clamp(idx, 0, 11))];
    }
    std::string GetCustomProperty(const std::string& key) const {
        auto it = customProps_.find(key);
        return (it != customProps_.end()) ? it->second : "";
    }
    const std::unordered_map<std::string, std::string>& CustomProperties() const {
        return customProps_;
    }

private:
    int primaryResource_ = -1;
    int secondaryResource_ = -1;
    int supplyUsed_ = -1;
    int supplyCap_ = -1;
    int upkeepLevel_ = -1;
    std::string upkeepLabel_;
    int selectedHandle_ = -1;
    std::string selectedName_;
    int hp_ = -1;
    int maxHp_ = -1;
    int energy_ = -1;
    int maxEnergy_ = -1;
    HudCommandSlotView commandSlots_[12]{};
    std::unordered_map<std::string, std::string> customProps_;
    uint32_t dirtyMask_ = HudFlagBits(HudDirtyFlags::All);
};

class IHudViewListener {
public:
    virtual ~IHudViewListener() = default;
    virtual void OnHudResourcesChanged(const HudViewModel& /*vm*/) {}
    virtual void OnHudUpkeepChanged(const HudViewModel& /*vm*/) {}
    virtual void OnHudSelectionChanged(const HudViewModel& /*vm*/) {}
    virtual void OnHudVitalsChanged(const HudViewModel& /*vm*/) {}
    virtual void OnHudCommandCardChanged(const HudViewModel& /*vm*/) {}
    virtual void OnHudCustomPropertiesChanged(const HudViewModel& /*vm*/) {}
};

class HudViewBinder {
public:
    void AttachListener(IHudViewListener* listener) {
        if (!listener) return;
        if (std::find(listeners_.begin(), listeners_.end(), listener) == listeners_.end()) {
            listeners_.push_back(listener);
        }
    }

    void DetachListener(IHudViewListener* listener) {
        listeners_.erase(std::remove(listeners_.begin(), listeners_.end(), listener), listeners_.end());
    }

    size_t ListenerCount() const { return listeners_.size(); }
    uint64_t DispatchCount() const { return dispatchCount_; }

    uint32_t Dispatch(HudViewModel& vm) {
        const uint32_t dirty = vm.ConsumeDirtyMask();
        if (dirty == 0u) return 0u;
        ++dispatchCount_;

        for (auto* l : listeners_) {
            if (!l) continue;
            if (dirty & HudFlagBits(HudDirtyFlags::Resources)) l->OnHudResourcesChanged(vm);
            if (dirty & HudFlagBits(HudDirtyFlags::Upkeep))    l->OnHudUpkeepChanged(vm);
            if (dirty & HudFlagBits(HudDirtyFlags::Selection)) l->OnHudSelectionChanged(vm);
            if (dirty & HudFlagBits(HudDirtyFlags::Vitals))    l->OnHudVitalsChanged(vm);
            if (dirty & HudFlagBits(HudDirtyFlags::CommandCard)) l->OnHudCommandCardChanged(vm);
            if (dirty & HudFlagBits(HudDirtyFlags::CustomProps)) l->OnHudCustomPropertiesChanged(vm);
        }
        return dirty;
    }

private:
    std::vector<IHudViewListener*> listeners_;
    uint64_t dispatchCount_ = 0;
};

} // namespace engine::ui
