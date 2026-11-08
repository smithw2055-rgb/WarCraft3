#include "core/input_action_map.h"

#include <algorithm>
#include <cmath>
#include <sstream>

#include "ecs/unit.h"
#include "ecs/behaviors/behavior_move.h"

namespace core {

void InputActionMap::RegisterStandardWarcraftDefaults() {
    bindings_.clear();
    insertionOrder_.clear();

    // Standard Warcraft 3 command card hotkeys (ASCII uppercase key codes)
    BindAction("order_stop",   InputKeyChord{'S', kInputModNone}, InputKeyChord{}, OrderIds::STOP);
    BindAction("order_hold",   InputKeyChord{'H', kInputModNone}, InputKeyChord{}, OrderIds::HOLD_POSITION);
    BindAction("order_move",   InputKeyChord{'M', kInputModNone}, InputKeyChord{}, OrderIds::MOVE);
    BindAction("order_patrol", InputKeyChord{'P', kInputModNone}, InputKeyChord{}, OrderIds::PATROL);
    BindAction("order_attack", InputKeyChord{'A', kInputModNone}, InputKeyChord{}, OrderIds::ATTACK);

    // Control groups 1..9 (Select: '1'..'9', Assign: Ctrl + '1'..'9')
    for (int g = 1; g <= 9; ++g) {
        const int key = '0' + g;
        BindAction("control_group_select_" + std::to_string(g),
                   InputKeyChord{key, kInputModNone});
        BindAction("control_group_assign_" + std::to_string(g),
                   InputKeyChord{key, kInputModCtrl});
    }
}

void InputActionMap::BindAction(const std::string& actionName,
                                InputKeyChord primary,
                                InputKeyChord secondary,
                                int associatedOrderId) {
    if (actionName.empty()) return;
    if (bindings_.find(actionName) == bindings_.end()) {
        insertionOrder_.push_back(actionName);
    }
    InputActionBinding b;
    b.actionName = actionName;
    b.primaryChord = primary;
    b.secondaryChord = secondary;
    b.associatedOrderId = associatedOrderId;
    bindings_[actionName] = std::move(b);
}

bool InputActionMap::RebindPrimary(const std::string& actionName, InputKeyChord newPrimary) {
    auto it = bindings_.find(actionName);
    if (it == bindings_.end()) return false;
    it->second.primaryChord = newPrimary;
    return true;
}

bool InputActionMap::RebindSecondary(const std::string& actionName, InputKeyChord newSecondary) {
    auto it = bindings_.find(actionName);
    if (it == bindings_.end()) return false;
    it->second.secondaryChord = newSecondary;
    return true;
}

const InputActionBinding* InputActionMap::FindAction(const std::string& actionName) const {
    auto it = bindings_.find(actionName);
    return (it != bindings_.end()) ? &it->second : nullptr;
}

int InputActionMap::ResolveTriggeredActions(int keyCode,
                                            uint8_t modifiers,
                                            std::vector<std::string>& outActions) const {
    outActions.clear();
    if (keyCode == 0) return 0;
    const InputKeyChord probe{keyCode, modifiers};
    for (const std::string& name : insertionOrder_) {
        auto it = bindings_.find(name);
        if (it == bindings_.end()) continue;
        const InputActionBinding& b = it->second;
        if ((b.primaryChord.IsValid() && b.primaryChord == probe) ||
            (b.secondaryChord.IsValid() && b.secondaryChord == probe)) {
            outActions.push_back(b.actionName);
        }
    }
    return static_cast<int>(outActions.size());
}

std::vector<std::pair<std::string, std::string>> InputActionMap::FindChordConflicts() const {
    std::vector<std::pair<std::string, std::string>> conflicts;
    for (size_t i = 0; i < insertionOrder_.size(); ++i) {
        auto itA = bindings_.find(insertionOrder_[i]);
        if (itA == bindings_.end()) continue;
        const InputActionBinding& a = itA->second;

        for (size_t j = i + 1; j < insertionOrder_.size(); ++j) {
            auto itB = bindings_.find(insertionOrder_[j]);
            if (itB == bindings_.end()) continue;
            const InputActionBinding& b = itB->second;

            bool clash = false;
            if (a.primaryChord.IsValid() &&
                (a.primaryChord == b.primaryChord || a.primaryChord == b.secondaryChord)) {
                clash = true;
            }
            if (a.secondaryChord.IsValid() &&
                (a.secondaryChord == b.primaryChord || a.secondaryChord == b.secondaryChord)) {
                clash = true;
            }
            if (clash) {
                conflicts.push_back({a.actionName, b.actionName});
            }
        }
    }
    return conflicts;
}

std::string InputActionMap::SerializeToText() const {
    std::ostringstream oss;
    for (const std::string& name : insertionOrder_) {
        auto it = bindings_.find(name);
        if (it == bindings_.end()) continue;
        const InputActionBinding& b = it->second;
        oss << b.actionName << "="
            << b.primaryChord.keyCode << ":" << static_cast<int>(b.primaryChord.modifiers) << ","
            << b.secondaryChord.keyCode << ":" << static_cast<int>(b.secondaryChord.modifiers) << ","
            << b.associatedOrderId << "\n";
    }
    return oss.str();
}

bool InputActionMap::DeserializeFromText(const std::string& text) {
    bindings_.clear();
    insertionOrder_.clear();

    std::istringstream iss(text);
    std::string line;
    while (std::getline(iss, line)) {
        if (line.empty()) continue;
        const size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        const std::string name = line.substr(0, eq);
        const std::string rhs = line.substr(eq + 1);

        int pk = 0, pm = 0, sk = 0, sm = 0, ord = 0;
        char c1 = 0, c2 = 0, c3 = 0, c4 = 0;
        std::istringstream rss(rhs);
        if (!(rss >> pk >> c1 >> pm >> c2 >> sk >> c3 >> sm >> c4 >> ord)) {
            return false;
        }
        BindAction(name,
                   InputKeyChord{pk, static_cast<uint8_t>(pm)},
                   InputKeyChord{sk, static_cast<uint8_t>(sm)},
                   ord);
    }
    return !bindings_.empty();
}

std::vector<FormationSlotOffset> CommandBufferPipeline::ComputeBoxFormationOffsets(
    size_t unitCount,
    float spacing,
    float facingRadians) {
    std::vector<FormationSlotOffset> offsets;
    if (unitCount == 0) return offsets;
    offsets.reserve(unitCount);
    if (unitCount == 1) {
        offsets.push_back({0.0f, 0.0f});
        return offsets;
    }

    const int cols = std::max(1, static_cast<int>(std::ceil(std::sqrt(static_cast<double>(unitCount)))));
    const int rows = static_cast<int>((unitCount + static_cast<size_t>(cols) - 1) / static_cast<size_t>(cols));

    const float cosA = std::cos(facingRadians);
    const float sinA = std::sin(facingRadians);
    // Forward axis = (cosA, sinA), Right axis = (sinA, -cosA)
    const float rightX =  sinA;
    const float rightY = -cosA;
    const float fwdX   =  cosA;
    const float fwdY   =  sinA;

    for (size_t idx = 0; idx < unitCount; ++idx) {
        const int r = static_cast<int>(idx) / cols;
        const int c = static_cast<int>(idx) % cols;
        const int colsInThisRow = (r == rows - 1)
            ? std::max(1, static_cast<int>(unitCount) - r * cols)
            : cols;

        const float lateralCoord = (static_cast<float>(c) - 0.5f * static_cast<float>(colsInThisRow - 1)) * spacing;
        const float depthCoord   = (0.5f * static_cast<float>(rows - 1) - static_cast<float>(r)) * spacing;

        FormationSlotOffset off;
        off.dx = rightX * lateralCoord + fwdX * depthCoord;
        off.dy = rightY * lateralCoord + fwdY * depthCoord;
        offsets.push_back(off);
    }
    return offsets;
}

int CommandBufferPipeline::IssueGroupPointOrder(CSimulation& sim,
                                                int playerIndex,
                                                const std::vector<int>& unitHandles,
                                                int orderId,
                                                float targetX,
                                                float targetY,
                                                bool useFormation,
                                                float spacing,
                                                ReplayLog* replayLog) {
    std::vector<CUnit*> validUnits;
    validUnits.reserve(unitHandles.size());
    for (int h : unitHandles) {
        CUnit* u = sim.GetUnit(h);
        if (u && !u->IsDead() && !u->IsHidden()) {
            validUnits.push_back(u);
        }
    }
    if (validUnits.empty()) return 0;

    // Sort by handle ID for 100% lockstep determinism
    std::sort(validUnits.begin(), validUnits.end(), [](const CUnit* a, const CUnit* b) {
        return a->GetHandleId() < b->GetHandleId();
    });

    // Compute group centroid to orient the box formation along travel vector
    float cx = 0.0f, cy = 0.0f;
    for (const CUnit* u : validUnits) {
        cx += u->GetX();
        cy += u->GetY();
    }
    cx /= static_cast<float>(validUnits.size());
    cy /= static_cast<float>(validUnits.size());

    const float facingRad = std::atan2(targetY - cy, targetX - cx);
    std::vector<FormationSlotOffset> offsets;
    if (useFormation && validUnits.size() > 1) {
        offsets = ComputeBoxFormationOffsets(validUnits.size(), spacing, facingRad);
        stats_.formationOrdersComputed++;
    } else {
        offsets.assign(validUnits.size(), FormationSlotOffset{0.0f, 0.0f});
    }

    int dispatched = 0;
    for (size_t i = 0; i < validUnits.size(); ++i) {
        const float destX = targetX + offsets[i].dx;
        const float destY = targetY + offsets[i].dy;
        const int uHandle = validUnits[i]->GetHandleId();

        if (sim.IssueUnitOrder(uHandle, std::make_unique<COrderTargetPoint>(orderId, destX, destY))) {
            if (useFormation && validUnits.size() > 1) {
                if (CBehaviorMove* mb = static_cast<CBehaviorMove*>(validUnits[i]->GetMoveBehavior())) {
                    mb->SetUseFlowFieldSteering(true);
                }
            }
            dispatched++;
            stats_.totalUnitCommandsDispatched++;
            if (replayLog) {
                SimCommand cmd;
                cmd.turn = sim.GetGameTurnTick();
                cmd.player = playerIndex;
                cmd.unit = uHandle;
                cmd.orderId = orderId;
                cmd.targetKind = static_cast<int32_t>(COrder::TargetKind::POINT);
                cmd.targetHandle = -1;
                cmd.x = destX;
                cmd.y = destY;
                cmd.z = 0.0f;
                replayLog->Record(cmd);
            }
        }
    }
    if (dispatched > 0) {
        stats_.totalGroupOrdersIssued++;
    }
    return dispatched;
}

int CommandBufferPipeline::IssueGroupImmediateOrder(CSimulation& sim,
                                                    int playerIndex,
                                                    const std::vector<int>& unitHandles,
                                                    int orderId,
                                                    ReplayLog* replayLog) {
    std::vector<int> sortedHandles = unitHandles;
    std::sort(sortedHandles.begin(), sortedHandles.end());

    int dispatched = 0;
    for (int h : sortedHandles) {
        if (sim.IssueUnitOrder(h, std::make_unique<COrderNoTarget>(orderId))) {
            dispatched++;
            stats_.totalUnitCommandsDispatched++;
            if (replayLog) {
                SimCommand cmd;
                cmd.turn = sim.GetGameTurnTick();
                cmd.player = playerIndex;
                cmd.unit = h;
                cmd.orderId = orderId;
                cmd.targetKind = static_cast<int32_t>(COrder::TargetKind::NONE);
                cmd.targetHandle = -1;
                replayLog->Record(cmd);
            }
        }
    }
    if (dispatched > 0) {
        stats_.totalGroupOrdersIssued++;
    }
    return dispatched;
}

} // namespace core
