# 现代化游戏引擎底座抽象与精简重构设计文档 (Phase 12 – Phase 19)

本项目以 **WarCraft III (Warsmash C++)** 为实验载体，参考 **Godot 4**（`OS / MainLoop`、Server 单例架构、`GDExtension` 动态插件系统、`SceneTree`、`ResourceServer`）与 **Unity**（`PlayerLoop`、`SubsystemManager`、`DOTS Sparse-Set ECS`、可热插拔 `GamePlay Assembly`、数据驱动 `ScriptableObject`）的设计哲学，在已完成 Phase 1–11 现代化改造的基础上，进一步完成：

* **Phase 12**：**通用游戏引擎底座（`engine::*`）抽象解耦** 与 **全库去冗余/过时代码精简（De-Redundancy & Legacy Pruning）**。

* **Phase 13**：**将 `CSimulation` 内的魔兽争霸3单位/技能体系彻底拆分为独立的 `war3::War3GameModule` 动态可插拔模块（含 `engine::ISimulationModule` 模拟策略接口、模块作用域技能目录管理与 OS 原生 `.so`/`.dll` `dlopen` 动态热重载器 `engine::DynamicModuleLoader`）**。

* **Phase 14**：**继续将 `CPlayer` 科技树/升级体系（Techtree Tier Equivalences & Live Upgrade Effect Engine）、维护费税率（Upkeep Tax）与对战/同盟胜负及残局暴露判定规则（Melee & Allied Victory/Defeat & Crippled Rules）进一步抽离至 `war3::War3GameModule` 动态插件中**。

* **Phase 15**：**继续将 `CCombatSystem` 7×8 护甲/攻击类型克制表、0.06 护甲减伤曲线、虚无/魔免规则，以及 `MeleeAIController` 战术 AI 生命周期托管、四族建造规划推导与护甲克制优先集火评分（Armor-Counter Tactical Target Scoring）全面解耦至 `war3::War3GameModule` 动态插件中**。

* **Phase 16**：**完成 `CUnit`、数据层 `CUnitData`、物品属性管线 `item_bonuses`、战役跨图英雄继承 `hero_carry` 与核心头文件体系的全面去耦合与精简：彻底移除 `src/ecs/unit.cpp` 内全部 10 个具体魔兽技能头文件（`unit.cpp` 代码量从 2,017 行精简至 1,196 行，缩减 40.7%），将 `CUnit` 16 个散落布尔字段压缩为 2 字节 `uint16_t` 位域标志（`UnitStateFlag`，内存占用缩减 87.5%），清理 `src/data/unitdata.cpp` 残留的 ECS 越层死代码，并将伤害前置/后置反应、受击被动触发、击杀赏金骰子、英雄经验分配、空闲自动施法、物品三维属性加成与战役英雄跨图快照全面收归 `war3::War3GameModule` (Version 16)**。

* **Phase 17**：**完成 `src/ecs/` 核心层与 `src/ecs/behaviors/*` 全部 6 大行为状态机的彻底去耦合（零具体技能头文件依赖），并对 `src/ecs/abilities/spells.cpp`..`spells25.cpp` 及金矿技能 `ability_gold_mine.cpp` 实施声明式效果原语（Declarative Spell Effect Primitives）合并去重：将 `src/ecs/` 顶层及 `src/ecs/behaviors/` 对 `ecs/abilities/*` 的 `#include` 依赖从 14 处清零至 0 处；同时在 `CAbilitySpellBase` 中提炼 8 大声明式法术效果原语与统一攻击/物品属性组件（`CAttackBonusDamageEffect`、`CAbilityItemStatBonus::StatSlot`），将 `War3GameModule` 升级至 Version 17（新增 `RegisterCustomSpellDescriptor`）**。

* **Phase 18**：**完成 `CBuff` 全家族 CRTP 自动克隆（`CBuffCloneable<Derived, Base>`）与状态/周期 AoE 辅助内核合并去重（`src/ecs/buffs.cpp` 从 1,257 行精简至 567 行，缩减 54.9%），对 `CAbility`（40 -> 24 Bytes，-40%）、`CBuff`（56 -> 40 Bytes，-28.6%）与 `CAbilitySpellBase`（72 -> 56 Bytes，-22.2%）实施紧凑位域压缩，继续精简剩余法术批次 `spells3`–`spells29`，并将 `War3GameModule` 升级至 Version 18（新增动态自动施法注册接口 `RegisterAutocastAbility` 与 `GetAbilityAutocastKind`）**。

* **Phase 19**：**完成脚本层（`src/scripting/*`）、UI 层（`src/ui/*`）、核心控制层（`src/core/*`）、地图加载层（`src/map/*`）与对战 AI（`src/ecs/melee_ai.cpp`）对具体魔兽技能头文件的彻底去耦合与 JASS Native 绑定精简：在 `CAbility` 基类上完善训练队列、建造列表、建筑升级、集结点、背包槽位扩展、英雄成长/三维属性及商店库存虚能力契约，并在 `War3GameModule`（升级至 Version 19）上新增建筑网格吸附与占地阻挡校验契约（`OnSnapBuildPosition` / `IsBuildLocationObstructed`），使 `src/scripting/`（7 -> 0）、`src/ui/`（10 -> 0）、`src/core/`（9 -> 0）、`src/map/`（1 -> 0）与 `src/ecs/`（3 -> 0）共 30 处具体技能头文件 `#include` 全部清零；同时提炼 `ResolveJassUnit`、`FindHeroAbility`、`FindUnitInventory`、`FindUnitShop`、`FindAllWorldShops` 消除 JASS Native 中的重复句柄解包与 `dynamic_cast` 样板**。

## 1. 通用化引擎底座架构抽象（`src/engine/`）

为了将本项目从“单一硬编码的魔兽争霸3模拟器”演进为“可复用、可插拔的通用 RTS/RPG 现代游戏引擎”，Phase 12–19 抽离了零业务耦合的 `src/engine/` 引擎内核层，将底层服务与上层游戏规则（`IGameModule` + `ISimulationModule`）彻底解耦：

| 引擎底座模块 | 核心职责与设计模式 | 对标现代引擎参考 | 
| ----- | ----- | ----- | 
| `engine::ISubsystem` & `engine::SubsystemRegistry` (`src/engine/engine_subsystem.h`) | 定义 9 阶段确定性生命周期总线（`Platform(0) -> Jobs(50) -> Resource(100) -> Audio(200) -> Scripting(300) -> Simulation(400) -> Presentation(500) -> Render(600) -> UI(700) -> Telemetry(800)`），提供 `O(1)` 类型索引服务定位器（`Register<T>` / `TryGet<T>`）、按阶段升序初始化与逆序安全销毁 | Unity `SubsystemManager` / Godot `Server` 注册表 | 
| `engine::FixedTimestepStepper` (`src/engine/fixed_timestep_stepper.h`) | 将固定频率确定性模拟步进（如 20Hz RTS 帧同步或 60Hz 物理步进）与高刷显示器渲染帧率（60/144/240Hz）彻底解耦；内置死亡螺旋钳制（`maxSubStepsPerFrame`）并自动计算 `[0, 1]` 帧间插值因子 `interpolationAlpha` | Godot `MainLoop` 物理插值分步器 / Unity `FixedUpdate` | 
| `engine::IGameModule` (`src/engine/game_module.h`) | 定义通用引擎底座与具体游戏业务模块（如 `War3GameModule`、塔防/MOBA/RPG 模块或无头回归测试宿主）的标准生命周期契约（`OnRegisterSubsystems`、`OnInit`、`OnFixedTick`、`OnFrameUpdate`、`OnRender`、`OnShutdown`） | Godot `MainLoop` / Unity `World` 业务插件 | 
| `engine::ISimulationModule` & `engine::NullSimulationModule` (`src/engine/simulation_module.h`) | **\[Phase 13–19 核心\]** 抽象 `CSimulation` 通用 ECS 世界容器、`CUnit` 通用实体、`CPlayer` 通用玩家状态、`CCombatSystem` 通用战斗计算器及 `MeleeAIController` 战术 AI 与具体游戏玩法规则的完整边界契约：涵盖技能目录挂载/卸载、单位原型技能装配与数据绑定、实体生命周期钩子、科技树等效层级解析、科技升级实时属性加成引擎、经济维护费税率、胜负与残局判定、攻击/护甲克制倍率与护甲减伤曲线、战术 AI 托管与护甲克制目标评分、伤害前置/后置修正、命中被动触发、击杀赏金与英雄经验分配、空闲自动施法与动态注册（`GetAbilityAutocastKind` / `RegisterAutocastAbility`）、物品属性路由、战役跨图英雄快照恢复、运行时声明式技能描述符动态注册契约（`RegisterCustomSpellDescriptor`），以及 **\[Phase 19 新增\] 建筑建造网格吸附与占地碰撞校验接口（`OnSnapBuildPosition` / `IsBuildLocationObstructed`）**。挂载 `NullSimulationModule` 时 `CSimulation` 即刻进入 **Pure Engine Mode（纯净通用引擎模式）** | Godot `GDExtension` 玩法扩展 / Unity `Authoring & Rule System` | 
| `engine::DynamicModuleLoader` (`src/engine/dynamic_module_loader.h/.cpp`) | **\[Phase 13 新增\]** 统一管理内置模块工厂（`RegisterBuiltInModule`）与操作系统原生共享库（`.so` / `.dll`）运行时动态加载（`LoadSharedLibrary`、`UnloadSharedLibrary`、`HotReloadSharedLibrary`），基于 POSIX `dlopen`/`dlsym`/`dlclose`（及 Windows `LoadLibraryA`/`GetProcAddress`）绑定 C ABI 导出入口，并通过 `shared_ptr` 自定义析构器安全托管跨 DLL 边界实例与代数（Generation）追踪 | Godot `GDExtensionLoader` / Unreal `FModuleManager` | 
| `engine::EngineKernel` (`src/engine/engine_kernel.h/.cpp`) | 统一收拢七大核心引擎服务器（`JobSystem`、`ResourceServer`、`AudioServer`、`RenderServer + RenderGraph`、`InputActionMap`、`ScriptHostRouter`、`EngineProfiler`），支持**独立自治模式（`BootstrapStandardSubsystems`）**与**非侵入宿主绑定模式（`BindExternalServices`）**双轨运行 | Godot `OS + Engine` / Unity `PlayerLoop` | 

## 2. Phase 13–18 核心解耦与精简里程碑回顾

* **Phase 13（`CSimulation` 单位/技能体系插件化）**：移除 `src/ecs/csimulation.cpp` 全部 14 个具体魔兽技能 `#include`，`src/ecs/ability_factory.cpp` 缩减 76%，支持 `libwar3_module_plugin.so` 动态加载与热重载。
* **Phase 14（`CPlayer` 科技树/升级与胜负判定插件化）**：封装四族科技树等效推导、`UpgradeData.slk` 实时属性升级引擎、维护费税率与对战/同盟胜负及残局暴露判定。
* **Phase 15（`CCombatSystem` 克制矩阵与 `MeleeAIController` 战术 AI 插件化）**：抽离 7×8 攻击/护甲克制表、0.06 护甲减伤曲线与战术 AI 护甲克制集火评分。
* **Phase 16（`CUnit` 零技能头文件耦合与 `UnitStateFlag` 位域压缩）**：移除 `src/ecs/unit.cpp` 全部 10 个具体魔兽技能头文件（`unit.cpp` 缩减 40.7%），将 `CUnit` 16 个布尔标志压缩为 2 字节 `uint16_t` 位域（缩减 87.5%）。
* **Phase 17（`src/ecs/` 与 `src/ecs/behaviors/*` 零具体技能头文件解耦 + 8 大声明式法术原语）**：`src/ecs/` 顶层及 `src/ecs/behaviors/*` 对具体技能头文件的 14 处 `#include` 清零；精简 18 个法术文件并支持 `RegisterCustomSpellDescriptor`。
* **Phase 18（`CBuffCloneable` CRTP 消重 + `CAbility`/`CBuff` 位域压缩 + 动态自动施法注册）**：消除全部 68 个 `CBuff` 派生类的手写 `Clone()`（`src/ecs/buffs.cpp` 缩减 54.9%），压缩 `CAbility`（24B）、`CBuff`（40B）、`CAbilitySpellBase`（56B）内存占用，并新增 `RegisterAutocastAbility`。

## 3. Phase 19：`src/scripting/`、`src/ui/`、`src/core/`、`src/map/` 与 `src/ecs/melee_ai.cpp` 具体技能头文件全面清零与 JASS Native 绑定精简

### 3.1 诊断与解耦目标

在完成 Phase 18 后，核心 ECS 模拟层已大幅解耦，但外围引擎子系统仍直接 `#include` 了 **30 处** `ecs/abilities/ability_*.h` 具体技能头文件，并存在大量 `dynamic_cast` 与重复句柄查询样板：

1. **UI 层（`src/ui/MainUI.cpp`）包含 10 个具体技能头文件**：
   * 直接包含了 `ability_attack.h`、`ability_build.h`、`ability_harvest.h`、`ability_inventory.h`、`ability_move.h`、`ability_queue.h`、`ability_rally.h`、`ability_repair.h`、`ability_spell_base.h`、`ability_upgrade.h`，并通过 11 处 `dynamic_cast<CAbility*>` 构建命令面板、训练队列与物品栏。

2. **核心控制层（`src/core/game_controller_internal.h`、`game_bootstrap_diagnostics.cpp`、`input_selection_controller.cpp`）包含 9 个具体技能头文件**：
   * `game_controller_internal.h` 包含了 9 个具体技能头文件，仅为了在寻找测试建造点时调用 `CAbilityBuild::RoundBuildPositionForType` 与 `IsBuildLocationObstructed`；
   * `input_selection_controller.cpp` 对 `Aatk`/`Ahar`/`Arep`/`Amov` 执行了 5 处不必要的 `dynamic_cast`（实际仅调用基类 `CAbility::CheckCanTarget`）。

3. **脚本层（`src/scripting/jass_natives_internal.h` 与 `jass_natives_*.cpp`）包含 7 个具体技能头文件与重复 JASS 句柄解包代码**：
   * `jass_natives_internal.h` 包含了 `ability_inventory.h`、`ability_hero.h`、`ability_harvest.h`、`ability_spell_base.h`、`ability_queue.h`、`ability_sell_items.h`、`ability_rally.h`；
   * `jass_natives_units.cpp` 中 14 个物品栏 JASS Native 与 15 个英雄 JASS Native 反复手写相同的 `data->GetSimHandle(...) -> sim->GetUnit(...) -> dynamic_cast<CAbilityInventory* / CAbilityHero*>` 样板。

4. **地图加载层（`src/map/map_loader.cpp`）与对战 AI（`src/ecs/melee_ai.cpp`）残留 4 处具体技能头文件**：
   * `map_loader.cpp` 包含 `ability_inventory.h` 处理预设物品槽拾取；`melee_ai.cpp` 包含 `ability_gold_mine.h`、`ability_build.h`、`ability_queue.h` 处理建造点查找与训练队列检查。

### 3.2 Phase 19 核心架构改造与精简成果

| 重构维度 | 具体改造内容 | 量化收益与通用引擎价值 |
| ----- | ----- | ----- |
| **`CAbility` 完整虚能力契约收口（`src/ecs/ability.h/.cpp`）** | 将 `CSpellTargetType`、`CQueueItemType`、`HeroStatValue` 上提至 `src/ecs/ability.h`，并在 `CAbility` 上补齐训练队列（`QueueUnit`/`QueueResearch`/`CancelQueueItem`/`GetQueueCount`/`GetQueuedItem`/`GetQueuedItemType`/`GetProgress`/`GetBuildTime`）、建造与升级列表（`GetBuildableStructures`/`GetTrainedUnits`/`GetResearches`/`GetUpgradesTo`）、集结点（`GetRallyTarget`/`HasRallyPoint`/`IsExplicitlySet`）、背包扩展（`UseItem`/`MoveItemToSlot`/`HasItem`/`GetItemCapacity`）、英雄属性与技能点（`GetHeroLevel`/`SetHeroLevel`/`GetXP`/`AddXP`/`SetXP`/`StripLevels`/`SelectHeroSkill`/`GetStrength`/`GetAgility`/`GetIntelligence`/`GetProperName`）及商店库存（`AddStockItem`/`AddStockUnit`/`RemoveStock`）虚接口 | **`src/ui/`（10 -> 0）、`src/core/`（9 -> 0）、`src/scripting/`（7 -> 0）、`src/map/`（1 -> 0）、`src/ecs/melee_ai.cpp`（3 -> 0）共 30 处具体技能头文件 `#include` 全部清零**；整个引擎除 `src/game/war3/` 与 `src/ecs/abilities/` 外再无任何具体技能头文件依赖 |
| **`War3GameModule` Version 19 建造网格吸附与占地阻挡接口** | 将 `War3GameModule` 与 `libwar3_module_plugin.so` 升级至 **Version `19u`**，在 `ISimulationModule` 与 `War3GameModule` 上新增 `OnSnapBuildPosition` 与 `IsBuildLocationObstructed`，解耦 `src/core/game_controller_internal.h` 与 `src/ecs/melee_ai.cpp` 对 `ability_build.h` 的直接调用 | 不同 RTS 游戏模块可自定义各自建筑网格对齐粒度（如 32×32、64×64 或自由放置）与占地阻挡规则 |
| **JASS Native 句柄解析与技能桥接精简（`jass_natives_internal.h` & `jass_natives_*.cpp`）** | 在 `jass_natives_internal.h` 中提炼 `ResolveJassUnit`、`FindHeroAbility`、`FindUnitInventory`、`FindUnitShop`、`FindAllWorldShops` 5 个内联辅助函数，重构 `jass_natives_units.cpp`、`jass_natives_ui_data.cpp`、`jass_natives_triggers.cpp` 与 `JassNatives.cpp` 中的英雄、背包、商店、训练队列与技能冷却 Native | 消除脚本层全部 `dynamic_cast<CAbility*>` 转换，并将 `UnitUseItem` / `UnitUseItemPoint` / `UnitUseItemTarget` 等重复 Native 实现合并复用 |

## 4. 云端游戏资源验证与单元测试覆盖

结合云端 Google Drive [`WarCraft3`](https://drive.google.com/drive/folders/1sEyljyjTcnwtxQNPEhYC2dVMh4Idit-L) / [`Maps/FrozenThrone`](https://drive.google.com/drive/folders/1sb1fktbnjqbIEjNrKky4l_iWyMfb_wU3) 下的 14 张零售级对战与 [`Scenario`](https://drive.google.com/drive/folders/18rVJURpX4ZHWLumjNUp0ZeXEN-TJOaj-) 地图（含 [`(2)EchoIsles.w3x`](https://drive.google.com/file/d/1d5qijwX3yNOvm9Xej64YpfSTivPlNGff/view?usp=drivesdk)、[`(4)TurtleRock.w3x`](https://drive.google.com/file/d/106f_pFaSePSYhTJTZezXX4NYGotoG1pY/view?usp=drivesdk)、[`(4)TwistedMeadows.w3x`](https://drive.google.com/file/d/1v51wDc9ilk5BtPbEsPUX0mZVWnA3olix/view?usp=drivesdk)、[`(8)GardenOfWar.w3x`](https://drive.google.com/file/d/1LOvXHMDAyjLaLdZlAg4ubMIAcqNi6qrP/view?usp=drivesdk)、[`(6)BlizzardTD.w3x`](https://drive.google.com/file/d/1NK4On-ccrEEzFzRPsTv625yZY5QXl-h8/view?usp=drivesdk)、[`(8)AzerothGrandPrix.w3x`](https://drive.google.com/file/d/1H-h7ZYHa2qdwykkdNkpSaRDdKuNojLAM/view?usp=drivesdk) 等），全量运行 **845 项** 单元与集成测试：

* **Phase 12–18 专项测试（`Stage EE1`–`Stage SS2`）**：28 项全通过。

* **Phase 19 专项测试**：

  * `Stage TT1`: 验证通过基类 `CAbility*` 指针操作建造菜单（`GetBuildableStructures`）、修理标志（`IsRepairAbility`）、训练队列（`QueueUnit`/`GetQueueCount`/`GetQueuedItem`/`CancelQueueItem`）、建筑升级（`GetUpgradesTo`）与集结点（`IsExplicitlySet`）虚契约。

  * `Stage TT2`: 验证通过基类 `CAbility*` 指针操作英雄成长/属性/技能学习/降级（`SetHeroLevel`/`SelectHeroSkill`/`AddStrengthBonus`/`StripLevels`/`SetProperName`）与商店库存管理（`AddStockItem`/`AddStockUnit`/`RemoveStock`）虚契约。

  * `Stage UU1`: 验证零具体技能头文件耦合下的 JASS Native（`GetHeroLevel`、`SetHeroLevel`、`ModifyHeroStat`、`GetHeroStr`、`UnitInventorySize`、`UnitAddItemById`、`UnitHasItem`、`StartUnitAbilityDefaultCooldown`、`EndUnitAbilityCooldown`）与 `ResolveJassUnit` / `FindHeroAbility` / `FindUnitInventory` 端到端闭环。

  * `Stage UU2`: 验证 `War3GameModule` Version 19 的 `OnSnapBuildPosition` 与 `IsBuildLocationObstructed` 建造校验契约，以及动态加载 `./libwar3_module_plugin.so`（Version 19）与云端零售地图 `(2)EchoIsles.w3x`、`(4)TwistedMeadows.w3x` 端到端验证。

* **总计 845 / 845 项测试全部通过（100% 通过率，零回归）**。
