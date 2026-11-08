#include "ability_factory.h"
#include "ecs/ability.h"
#include "ecs/abilities/ability_generic.h"
#include "ecs/abilities/ability_spell_base.h"
#include "ecs/unit.h"
#include "ecs/csimulation.h"
#include "game/war3/war3_game_module.h"
#include <unordered_map>

namespace {

struct CreatorEntry {
    AbilityFactory::CreatorFn fn;
    std::string moduleId;
};

struct RegistryStore {
    std::unordered_map<uint32_t, CreatorEntry> creators;
    bool autoBootstrapWar3 = true;
    bool war3Bootstrapped = false;
};

RegistryStore& GetStore() {
    static RegistryStore store;
    return store;
}

void MaybeBootstrapDefaultCatalog() {
    auto& store = GetStore();
    if (store.autoBootstrapWar3 && !store.war3Bootstrapped) {
        store.war3Bootstrapped = true;
        war3::War3GameModule::RegisterWar3AbilityCatalog();
    }
}

} // namespace

void AbilityFactory::SetAutoBootstrapDefaultCatalog(bool enabled) {
    GetStore().autoBootstrapWar3 = enabled;
}

bool AbilityFactory::IsAutoBootstrapDefaultCatalogEnabled() {
    return GetStore().autoBootstrapWar3;
}

void AbilityFactory::EnsureDefaultCatalogRegistered() {
    auto& store = GetStore();
    if (!store.war3Bootstrapped) {
        store.war3Bootstrapped = true;
        war3::War3GameModule::RegisterWar3AbilityCatalog();
    }
}

void AbilityFactory::RegisterCreator(War3ID code, CreatorFn creator) {
    RegisterModuleCreator("Global", code, std::move(creator));
}

void AbilityFactory::RegisterModuleCreator(const std::string& moduleId, War3ID code, CreatorFn creator) {
    if (moduleId == war3::War3GameModule::kModuleId) {
        GetStore().war3Bootstrapped = true;
    } else {
        MaybeBootstrapDefaultCatalog();
    }
    GetStore().creators[code.value] = CreatorEntry{std::move(creator), moduleId};
}

void AbilityFactory::RegisterAlias(War3ID alias, War3ID targetCode) {
    RegisterModuleAlias("Global", alias, targetCode);
}

void AbilityFactory::RegisterModuleAlias(const std::string& moduleId, War3ID alias, War3ID targetCode) {
    if (moduleId == war3::War3GameModule::kModuleId) {
        GetStore().war3Bootstrapped = true;
    } else {
        MaybeBootstrapDefaultCatalog();
    }
    auto& creators = GetStore().creators;
    auto it = creators.find(targetCode.value);
    if (it != creators.end() && it->second.fn) {
        CreatorFn targetFn = it->second.fn;
        creators[alias.value] = CreatorEntry{
            [targetFn, targetCode](int handleId, War3ID aliasCode) -> std::unique_ptr<CAbility> {
                auto ab = targetFn(handleId, targetCode);
                if (ab) ab->SetAlias(aliasCode);
                return ab;
            },
            moduleId
        };
    }
}

void AbilityFactory::RegisterSpellDescriptor(const ABSpellDescriptor& desc) {
    RegisterModuleSpellDescriptor("Global", desc);
}

void AbilityFactory::RegisterModuleSpellDescriptor(const std::string& moduleId, const ABSpellDescriptor& desc) {
    if (moduleId == war3::War3GameModule::kModuleId) {
        GetStore().war3Bootstrapped = true;
    } else {
        MaybeBootstrapDefaultCatalog();
    }
    AbilityBuilderRegistry::Instance().RegisterDescriptor(desc);
    GetStore().creators[desc.code.value] = CreatorEntry{
        [desc](int handleId, War3ID code) -> std::unique_ptr<CAbility> {
            auto spell = std::make_unique<CAbilityDataDrivenSpell>(handleId, code, desc);
            spell->SetAlias(code);
            return spell;
        },
        moduleId
    };
}

bool AbilityFactory::UnregisterCreator(War3ID code) {
    AbilityBuilderRegistry::Instance().Unregister(code);
    return GetStore().creators.erase(code.value) > 0;
}

size_t AbilityFactory::UnregisterModuleCreators(const std::string& moduleId) {
    auto& store = GetStore();
    size_t removed = 0;
    for (auto it = store.creators.begin(); it != store.creators.end();) {
        if (it->second.moduleId == moduleId) {
            AbilityBuilderRegistry::Instance().Unregister(War3ID(it->first));
            it = store.creators.erase(it);
            ++removed;
        } else {
            ++it;
        }
    }
    if (moduleId == war3::War3GameModule::kModuleId && removed > 0) {
        store.war3Bootstrapped = false;
    }
    return removed;
}

size_t AbilityFactory::GetModuleRegisteredCount(const std::string& moduleId) {
    MaybeBootstrapDefaultCatalog();
    size_t count = 0;
    for (const auto& [code, entry] : GetStore().creators) {
        (void)code;
        if (entry.moduleId == moduleId) {
            ++count;
        }
    }
    return count;
}

std::string AbilityFactory::GetCreatorModuleId(War3ID code) {
    MaybeBootstrapDefaultCatalog();
    const auto& creators = GetStore().creators;
    auto it = creators.find(code.value);
    return (it != creators.end()) ? it->second.moduleId : "";
}

bool AbilityFactory::HasRegisteredCreator(War3ID code) {
    MaybeBootstrapDefaultCatalog();
    if (GetStore().creators.count(code.value) > 0) return true;
    if (GetStore().war3Bootstrapped && AbilityBuilderRegistry::Instance().FindDescriptor(code) != nullptr) return true;
    if (GetStore().war3Bootstrapped && AbilityBuilderRegistry::Instance().Find(code) != nullptr) return true;
    return false;
}

size_t AbilityFactory::GetRegisteredCount() {
    MaybeBootstrapDefaultCatalog();
    return GetStore().creators.size();
}

std::unique_ptr<CAbility> AbilityFactory::CreateAbility(int handleId, War3ID code) {
    MaybeBootstrapDefaultCatalog();

    // 1. Check O(1) registered creator table
    auto& creators = GetStore().creators;
    auto it = creators.find(code.value);
    if (it != creators.end() && it->second.fn) {
        return it->second.fn(handleId, code);
    }

    // 2. Check data-driven ABSpellDescriptor registry (only when War3 catalog is active)
    if (GetStore().war3Bootstrapped) {
        if (const ABSpellDescriptor* desc = AbilityBuilderRegistry::Instance().FindDescriptor(code)) {
            auto spell = std::make_unique<CAbilityDataDrivenSpell>(handleId, code, *desc);
            spell->SetAlias(code);
            return spell;
        }
    }

    // 3. Check raw ABEffectChain registry (only when War3 catalog is active)
    if (GetStore().war3Bootstrapped && AbilityBuilderRegistry::Instance().Find(code) != nullptr) {
        ABSpellDescriptor desc;
        desc.code = code;
        auto spell = std::make_unique<CAbilityDataDrivenSpell>(handleId, code, desc);
        spell->SetAlias(code);
        return spell;
    }

    // 4. Fallback generic do-nothing placeholder
    return std::make_unique<CAbilityGenericDoNothing>(handleId, code);
}

void AbilityFactory::AddDefaultAbilities(CSimulation& game, CUnit* unit,
                                         const std::vector<War3ID>& abilityList) {
    if (!unit) return;
    for (auto& code : abilityList) {
        if (code == War3ID("Aatk") || code == War3ID("Amov")) continue;
        if (unit->GetAbility(code)) continue;
        std::unique_ptr<CAbility> ability =
            CreateAbility(game.AllocateHandleId(), code);
        if (!ability) continue;
        unit->AddAbility(game, std::move(ability));
    }
}
