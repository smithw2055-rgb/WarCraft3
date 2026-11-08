#pragma once
#include "ecs/fwd.h"
#include "resource/slk.h"
#include <cstdint>
#include <string>

// ============================================================
// COrder - Unified Command & Order Representation
// Phase 20 Stage WW: Stores orderId, TargetKind, (x, y, z), and target
// directly in the base COrder object so CUnit::IssueOrder,
// CUnit::ConvertOrderToBehavior, and CSimulation::IssueUnitOrder
// dispatch in O(1) via GetTargetKind() without RTTI dynamic_cast.
// ============================================================
class COrder {
public:
    enum class TargetKind : int32_t { NONE = 0, POINT = 1, WIDGET = 2 };

    COrder(int orderId, TargetKind kind = TargetKind::NONE,
           float x = 0.0f, float y = 0.0f, float z = 0.0f,
           CWidget* target = nullptr)
        : orderId_(orderId), kind_(kind), x_(x), y_(y), z_(z), target_(target) {}
    virtual ~COrder() = default;

    int GetOrderId() const { return orderId_; }
    TargetKind GetTargetKind() const { return kind_; }
    float GetX() const { return x_; }
    float GetY() const { return y_; }
    float GetZ() const { return z_; }
    CWidget* GetTarget() const { return target_; }

protected:
    int orderId_ = 0;
    TargetKind kind_ = TargetKind::NONE;
    float x_ = 0.0f;
    float y_ = 0.0f;
    float z_ = 0.0f;
    CWidget* target_ = nullptr;
};

class COrderNoTarget : public COrder {
public:
    explicit COrderNoTarget(int orderId)
        : COrder(orderId, TargetKind::NONE) {}
};

class COrderTargetPoint : public COrder {
public:
    COrderTargetPoint(int orderId, float x, float y, float z = 0.0f)
        : COrder(orderId, TargetKind::POINT, x, y, z, nullptr) {}
};

class COrderTargetWidget : public COrder {
public:
    COrderTargetWidget(int orderId, CWidget* target)
        : COrder(orderId, TargetKind::WIDGET, 0.0f, 0.0f, 0.0f, target) {}
};

struct OrderIds {
    static constexpr int STOP = 851972;
    static constexpr int MOVE = 851986;
    static constexpr int ATTACK = 851983;
    static constexpr int ATTACK_GROUND = 851984;
    static constexpr int HOLD_POSITION = 851993;
    static constexpr int FOLLOW = 851972;
    static constexpr int PATROL = 851990;
    static constexpr int MOVE_ATTACK = 851983;

    // Smart order / right-click context
    static constexpr int SMART = 851971;
    static constexpr int MOVE_SMART = 851971;
    static constexpr int NEUTRALINTERACT = 852566;

    // Building
    static constexpr int CANCEL = 851976;
    static constexpr int CANCEL_BUILD = 851976;
    static constexpr int CANCEL_TRAIN = 851976;
    static constexpr int CANCEL_RESEARCH = 851976;

    // Race-specific build menus
    static constexpr int HUMAN_BUILD = 851995;
    static constexpr int ORC_BUILD = 851996;
    static constexpr int UNDEAD_BUILD = 851998;
    static constexpr int NIGHT_ELF_BUILD = 851997;

    // Harvest / Return Resources
    static constexpr int HARVEST = 852018;
    static constexpr int RETURN_RESOURCES = 852020;

    // Repair
    static constexpr int REPAIR = 852024;

    // Inventory
    static constexpr int ITEM_USE = 852008;
    static constexpr int ITEM_DROP = 852001;
    static constexpr int ITEM_GIVE = 852099;

    // Hero skill menu
    static constexpr int HERO_SKILL = 852000;
    static constexpr int SELECT_SKILL = 852000;

    // Rally
    static constexpr int SET_RALLY = 851980;
    static constexpr int RALLY = 851984;

    // Queue
    static constexpr int CANCEL_QUEUE = 851978;

    // Upgrade
    static constexpr int CANCEL_UPGRADE = 851976;

    // Generic order base
    static constexpr int ORDER_FIRST = 851971;
    static constexpr int ORDER_LAST = 852150;

    // Ability-based orders start at a higher range
    static constexpr int ABILITY_ORDER = 852000;
};

// ============================================================
// Retail + Dynamic Module Order Tables (Phase 20 Stage WW)
// ============================================================

// JASS order string ("whirlwind") -> retail or custom id (852128); 0 when unknown.
int OrderIdFromString(const std::string& order);
// Retail or custom id -> JASS order string; empty when unknown.
std::string OrderStringFromId(int orderId);
// Base order id for a spell ability code ("AOww" -> 852128 whirlwind);
// 0 when the ability has no engine order (passives, item powers).
int SpellBaseOrderId(War3ID code);
// Whether an incoming order id may trigger the spell: exact base-order
// match, secondary toggle pair, or legacy id 0.
bool SpellMatchesOrder(War3ID code, int orderId);

// Runtime custom order registration for pluggable game modules (Phase 20)
void RegisterCustomOrderString(const std::string& order, int orderId);
void RegisterAbilityOrderMapping(War3ID code, int baseOrderId, int secondaryOrderId = 0);
void ClearCustomOrderMappings();
