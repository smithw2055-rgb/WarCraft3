#pragma once
// ============================================================================
// Stage R & Phase 27: Declarative InputActionMap & Multi-Formation Role-Aware
// CommandBufferPipeline (Box, Line, Circle, Wedge + Frontline/Backline Sorting)
// ============================================================================
#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "ecs/csimulation.h"
#include "ecs/lockstep.h"
#include "ecs/orders/order.h"

namespace core {

enum InputModifierFlags : uint8_t {
    kInputModNone  = 0,
    kInputModShift = 1 << 0,
    kInputModCtrl  = 1 << 1,
    kInputModAlt   = 1 << 2
};

struct InputKeyChord {
    int keyCode = 0;
    uint8_t modifiers = kInputModNone;

    constexpr bool IsValid() const { return keyCode != 0; }
    constexpr bool operator==(const InputKeyChord& other) const {
        return keyCode == other.keyCode && modifiers == other.modifiers;
    }
    constexpr bool operator!=(const InputKeyChord& other) const {
        return !(*this == other);
    }
};

struct InputActionBinding {
    std::string actionName;
    InputKeyChord primaryChord{};
    InputKeyChord secondaryChord{};
    int associatedOrderId = 0;
};

class InputActionMap {
public:
    InputActionMap() = default;

    void RegisterStandardWarcraftDefaults();

    void BindAction(const std::string& actionName,
                    InputKeyChord primary,
                    InputKeyChord secondary = InputKeyChord{},
                    int associatedOrderId = 0);

    bool RebindPrimary(const std::string& actionName, InputKeyChord newPrimary);
    bool RebindSecondary(const std::string& actionName, InputKeyChord newSecondary);

    const InputActionBinding* FindAction(const std::string& actionName) const;

    // Resolve all action names triggered by (keyCode, modifiers)
    int ResolveTriggeredActions(int keyCode,
                                uint8_t modifiers,
                                std::vector<std::string>& outActions) const;

    // Detect any two distinct actions sharing an identical non-zero key chord
    std::vector<std::pair<std::string, std::string>> FindChordConflicts() const;

    // Human-readable config text serialization (`action=keyCode:mod,secKey:secMod,orderId`)
    std::string SerializeToText() const;
    bool DeserializeFromText(const std::string& text);

    size_t ActionCount() const { return bindings_.size(); }

private:
    std::unordered_map<std::string, InputActionBinding> bindings_;
    std::vector<std::string> insertionOrder_;
};

enum class FormationShape : uint8_t {
    Box    = 0,
    Line   = 1,
    Circle = 2,
    Wedge  = 3
};

struct FormationSlotOffset {
    float dx = 0.0f;
    float dy = 0.0f;
};

struct CommandPipelineStats {
    uint64_t totalGroupOrdersIssued = 0;
    uint64_t totalUnitCommandsDispatched = 0;
    uint64_t formationOrdersComputed = 0;
};

class CommandBufferPipeline {
public:
    CommandBufferPipeline() = default;

    // Compute rank-and-file box formation offsets oriented along `facingRadians`
    static std::vector<FormationSlotOffset> ComputeBoxFormationOffsets(
        size_t unitCount,
        float spacing = 72.0f,
        float facingRadians = 0.0f);

    // Phase 27: Compute formation offsets for Box, Line, Circle, or Wedge shapes
    static std::vector<FormationSlotOffset> ComputeFormationOffsets(
        size_t unitCount,
        FormationShape shape,
        float spacing = 72.0f,
        float facingRadians = 0.0f);

    // Issue a deterministic point order (MOVE, ATTACK, PATROL, SMART) across a
    // selection group, sorting units by (ISimulationModule::OnGetUnitFormationRankPriority, handleId)
    // so frontline tanks take the front slots and ranged/casters take the back slots.
    int IssueGroupPointOrder(CSimulation& sim,
                             int playerIndex,
                             const std::vector<int>& unitHandles,
                             int orderId,
                             float targetX,
                             float targetY,
                             bool useFormation = true,
                             float spacing = 72.0f,
                             ReplayLog* replayLog = nullptr,
                             FormationShape shape = FormationShape::Box);

    // Issue a deterministic no-target order (STOP, HOLDPOSITION) across a group
    int IssueGroupImmediateOrder(CSimulation& sim,
                                 int playerIndex,
                                 const std::vector<int>& unitHandles,
                                 int orderId,
                                 ReplayLog* replayLog = nullptr);

    void ResetStats() { stats_ = CommandPipelineStats{}; }
    const CommandPipelineStats& GetStats() const { return stats_; }

private:
    CommandPipelineStats stats_{};
};

} // namespace core
