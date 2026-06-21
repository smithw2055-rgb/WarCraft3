#pragma once
#include "ecs/fwd.h"
#include <cstdint>

class COrder {
public:
    virtual ~COrder() = default;
    virtual int GetOrderId() const = 0;
};

class COrderNoTarget : public COrder {
public:
    COrderNoTarget(int orderId) : orderId_(orderId) {}
    int GetOrderId() const override { return orderId_; }
private:
    int orderId_;
};

class COrderTargetPoint : public COrder {
public:
    COrderTargetPoint(int orderId, float x, float y, float z = 0)
        : orderId_(orderId), x_(x), y_(y), z_(z) {}
    int GetOrderId() const override { return orderId_; }
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
    static constexpr int HOLD_POSITION = 851973;
    static constexpr int FOLLOW = 851972;
    static constexpr int PATROL = 851990;
    static constexpr int MOVE_ATTACK = 851983;

    // Smart order / right-click context
    static constexpr int SMART = 851971;
    static constexpr int MOVE_SMART = 851971;

    // Building
    static constexpr int CANCEL = 851976;
    static constexpr int CANCEL_BUILD = 851994;
    static constexpr int CANCEL_TRAIN = 851994;
    static constexpr int CANCEL_RESEARCH = 851995;

    // Race-specific build menus
    static constexpr int HUMAN_BUILD = 852001;
    static constexpr int ORC_BUILD = 852002;
    static constexpr int UNDEAD_BUILD = 852003;
    static constexpr int NIGHT_ELF_BUILD = 852004;

    // Harvest / Return Resources
    static constexpr int HARVEST = 852010;
    static constexpr int RETURN_RESOURCES = 852011;

    // Build structure (when placing a building from the build menu)
    static constexpr int BUILD_HUMAN = 852020;
    static constexpr int BUILD_ORC = 852021;
    static constexpr int BUILD_UNDEAD = 852022;
    static constexpr int BUILD_NIGHT_ELF = 852023;

    // Inventory
    static constexpr int ITEM_USE = 852097;
    static constexpr int ITEM_DROP = 852098;
    static constexpr int ITEM_GIVE = 852099;

    // Hero
    static constexpr int HERO_SKILL = 852095;
    static constexpr int SELECT_SKILL = 852095;

    // Rally
    static constexpr int SET_RALLY = 851980;
    static constexpr int RALLY = 851984;

    // Queue
    static constexpr int CANCEL_QUEUE = 851978;

    // Upgrade
    static constexpr int CANCEL_UPGRADE = 851977;

    // Generic order base (most common orders start here)
    static constexpr int ORDER_FIRST = 851971;
    static constexpr int ORDER_LAST = 852150;

    // Ability-based orders start at a higher range
    static constexpr int ABILITY_ORDER = 852000;
};
