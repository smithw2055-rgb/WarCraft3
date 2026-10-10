#pragma once
#include "ecs/fwd.h"
#include "resource/slk.h"
#include <cstdint>
#include <string>

class COrder {
public:
    virtual ~COrder() = default;
    virtual int GetOrderId() const = 0;

    enum class TargetKind : int32_t { NONE = 0, POINT = 1, WIDGET = 2 };
    virtual TargetKind GetTargetKind() const = 0;
};

class COrderNoTarget : public COrder {
public:
    COrderNoTarget(int orderId) : orderId_(orderId) {}
    int GetOrderId() const override { return orderId_; }
    TargetKind GetTargetKind() const override { return TargetKind::NONE; }
private:
    int orderId_;
};

class COrderTargetPoint : public COrder {
public:
    COrderTargetPoint(int orderId, float x, float y, float z = 0)
        : orderId_(orderId), x_(x), y_(y), z_(z) {}
    int GetOrderId() const override { return orderId_; }
    TargetKind GetTargetKind() const override { return TargetKind::POINT; }
    float GetX() const { return x_; }
    float GetY() const { return y_; }
    float GetZ() const { return z_; }
private:
    int orderId_;
    float x_, y_, z_;
};

class COrderTargetWidget : public COrder {
public:
    COrderTargetWidget(int orderId, CWidget* target)
        : orderId_(orderId), target_(target) {}
    int GetOrderId() const override { return orderId_; }
    TargetKind GetTargetKind() const override { return TargetKind::WIDGET; }
    CWidget* GetTarget() const { return target_; }
private:
    int orderId_;
    CWidget* target_;
};

struct OrderIds {
    static constexpr int STOP = 851972;
    static constexpr int MOVE = 851986;
    static constexpr int ATTACK = 851983;
    static constexpr int ATTACK_GROUND = 851984;
    // Retail stunned is 851973; hold position is 851993 (ref OrderIds).
    static constexpr int HOLD_POSITION = 851993;
    static constexpr int FOLLOW = 851972;
    static constexpr int PATROL = 851990;
    static constexpr int MOVE_ATTACK = 851983;

    // Smart order / right-click context
    static constexpr int SMART = 851971;
    static constexpr int MOVE_SMART = 851971;
    static constexpr int NEUTRALINTERACT = 852566;

    // Building (retail ids, ref OrderIds: 851976 cancel; 851994 is the
    // build *menu*, not a cancel).
    static constexpr int CANCEL = 851976;
    static constexpr int CANCEL_BUILD = 851976;
    static constexpr int CANCEL_TRAIN = 851976;
    static constexpr int CANCEL_RESEARCH = 851976;

    // Race-specific build menus (retail ids, ref OrderIds humanbuild /
    // orcbuild / undeadbuild / nightelfbuild).
    static constexpr int HUMAN_BUILD = 851995;
    static constexpr int ORC_BUILD = 851996;
    static constexpr int UNDEAD_BUILD = 851998;
    static constexpr int NIGHT_ELF_BUILD = 851997;

    // Harvest / Return Resources (retail ids, ref OrderIds).
    static constexpr int HARVEST = 852018;
    static constexpr int RETURN_RESOURCES = 852020;

    // Repair (retail id, ref OrderIds).
    static constexpr int REPAIR = 852024;

    // NOTE: a previous BUILD_HUMAN/_ORC/_UNDEAD/_NIGHT_ELF quartet
    // (852020-852023) was removed here: unused anywhere, and each value
    // collides with a real retail order (returnresources/autoharvestgold/
    // autoharvestlumber/neutraldetectaoe). Build placement orders carry
    // the building FourCC as the order id (see CAbilityBuild).

    // Inventory (slot-0 retail ids; avoids colliding with ability orders
    // such as waterelemental=852097, ref OrderIds itemuse00/dropitem).
    static constexpr int ITEM_USE = 852008;
    static constexpr int ITEM_DROP = 852001;
    static constexpr int ITEM_GIVE = 852099;

    // Hero skill menu (retail skillmenu, ref OrderIds; 852095 is
    // thunderbolt, previously misassigned here).
    static constexpr int HERO_SKILL = 852000;
    static constexpr int SELECT_SKILL = 852000;

    // Rally
    static constexpr int SET_RALLY = 851980;
    static constexpr int RALLY = 851984;

    // Queue
    static constexpr int CANCEL_QUEUE = 851978;

    // Upgrade (in-game cancel button issues 851976 like every cancel).
    static constexpr int CANCEL_UPGRADE = 851976;

    // Generic order base (most common orders start here)
    static constexpr int ORDER_FIRST = 851971;
    static constexpr int ORDER_LAST = 852150;

    // Ability-based orders start at a higher range
    static constexpr int ABILITY_ORDER = 852000;
};

// ============================================================
// Retail order tables (see order.cpp, generated from ref
// OrderIds.java field names/values; JASS order strings are the
// lowercase field names, cf. ref OrderIdUtils).
// ============================================================

// JASS order string ("whirlwind") -> retail id (852128); 0 when unknown.
int OrderIdFromString(const std::string& order);
// Retail id -> JASS order string; empty when unknown.
std::string OrderStringFromId(int orderId);
// Base order id for a spell ability code ("AOww" -> 852128 whirlwind);
// 0 when the ability has no engine order (passives, item powers).
int SpellBaseOrderId(War3ID code);
// Whether an incoming order id may trigger the spell: exact base-order
// match, the Immolation toggle pair, or legacy id 0 (item use and
// pre-table callers pass 0; positional dispatch is preserved there).
bool SpellMatchesOrder(War3ID code, int orderId);
