#pragma once
#include "ecs/ability.h"

// CAbilityReturnResources - town hall ability to accept returned resources
class CAbilityReturnResources : public CAbility {
public:
    CAbilityReturnResources(int handleId);

    void OnAdd(CSimulation& game, CUnit* unit) override;

    // Configuration
    void SetAcceptsGold(bool v) { acceptsGold_ = v; }
    void SetAcceptsLumber(bool v) { acceptsLumber_ = v; }
    bool AcceptsGold() const { return acceptsGold_; }
    bool AcceptsLumber() const { return acceptsLumber_; }

private:
    bool acceptsGold_ = true;
    bool acceptsLumber_ = true;
};
