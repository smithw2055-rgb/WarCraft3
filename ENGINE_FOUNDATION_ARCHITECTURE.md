# 现代化游戏引擎底座抽象与精简重构设计文档 (Phase 12 – Phase 18)

本项目以 **WarCraft III (Warsmash C++)** 为实验载体，参考 **Godot 4**（`OS / MainLoop`、Server 单例架构、`GDExtension` 动态插件系统、`SceneTree`、`ResourceServer`）与 **Unity**（`PlayerLoop`、`SubsystemManager`、`DOTS Sparse-Set ECS`、可热插拔 `GamePlay Assembly`、数据驱动 `ScriptableObject`）的设计哲学，在已完成 Phase 1–11 现代化改造的基础上，进一步完成：

* **Phase 12**：**通用游戏引擎底座（`engine::*`）抽象解耦** 与 **全库去冗余/过时代码精简（De-Redundancy & Legacy Pruning）**。

* **Phase 13**：**将 `CSimulation` 内的魔兽争霸3单位/技能体系彻底拆分为独立的 `war3::War3GameModule` 动态可插拔模块（含 `engine::ISimulationModule` 模拟策略接口、模块作用域技能目录管理与 OS 原生 `.so`/`.dll` `dlopen` 动态热重载器 `engine::DynamicModuleLoader`）**。

* **Phase 14**：**继续将 `CPlayer` 科技树/升级体系（Techtree Tier Equivalences & Live Upgrade Effect Engine）、维护费税率（Upkeep Tax）与对战/同盟胜负及残局暴露判定规则（Melee & Allied Victory/Defeat & Crippled Rules）进一步抽离至 `war3::War3GameModule` 动态插件中**。

* **Phase 15**：**继续将 `CCombatSystem` 7×8 护甲/攻击类型克制表、0.06 护甲减伤曲线、虚无/魔免规则，以及 `MeleeAIController` 战术 AI 生命周期托管、四族建造规划推导与护甲克制优先集火评分（Armor-Counter Tactical Target Scoring）全面解耦至 `war3::War3GameModule` 动态插件中**。

* **Phase 16**：**完成 `CUnit`、数据层 `CUnitData`、物品属性管线 `item_bonuses`、战役跨图英雄继承 `hero_carry` 与核心头文件体系的全面去耦合与精简：彻底移除 `src/ecs/unit.cpp` 内全部 10 个具体魔兽技能头文件（`unit.cpp` 代码量从 2,017 行精简至 1,196 行，缩减 40.7%），将 `CUnit` 16 个散落布尔字段压缩为 2 字节 `uint16_t` 位域标志（`UnitStateFlag`，内存占用缩减 87.5%），清理 `src/data/unitdata.cpp` 残留的 ECS 越层死代码，并将伤害前置/后置反应、受击被动触发、击杀赏金骰子、英雄经验分配、空闲自动施法、物品三维属性加成与战役英雄跨图快照全面收归 `war3::War3GameModule` (Version 16)**。

* **Phase 17**：**完成 `src/ecs/` 核心层与 `src/ecs/behaviors/*` 全部 6 大行为状态机的彻底去耦合（零具体技能头文件依赖），并对 `src/ecs/abilities/spells.cpp`..`spells25.cpp` 及金矿技能 `ability_gold_mine.cpp` 实施声明式效果原语（Declarative Spell Effect Primitives）合并去重：将光环加成模型（`CAuraEffectType`/`CAuraBonus`）、种族建造契约（`CBuildRace`）与 26 项通用技能/采集/建造/修理/背包/金矿虚能力契约上提至 `src/ecs/ability.h`，使 `src/ecs/` 顶层及 `src/ecs/behaviors/` 对 `ecs/abilities/*` 的 `#include` 依赖从 14 处清零至 0 处；同时在 `CAbilitySpellBase` 中提炼 8 大声明式法术效果原语与统一攻击/物品属性组件（`CAttackBonusDamageEffect`、`CAbilityItemStatBonus::StatSlot`），将 `War3GameModule` 升级至 Version 17（新增运行时无子类注册声明式法术接口 `RegisterCustomSpellDescriptor`）**。

* **Phase 18**：**完成 `CBuff` / `CAbility` / `CAbilitySpellBase` 核心实例内存布局位域压缩、CRTP 零样板深拷贝模板（`CBuffCloneable<Derived, Base>`）、多 Buff 叠加状态引用计数安全恢复与 Batch 26–29 法术体系去冗余精简：将 `sizeof(CBuff)` 从 56B 压缩至 40B（缩减 28.6%）、`sizeof(CAbility)` 从 40B 压缩至 32B（缩减 20%）、`sizeof(CAbilitySpellBase)` 从 72B 压缩至 56B（缩减 22.2%）；用 `CBuffCloneable` 消除 68 个具体 Buff 子类中全部手写 `Clone()` 样板代码；修复多重同状态 Buff（无敌/定身/隐身/魔免）叠加移除时误清底层单位状态的架构隐患；提炼 `SetPassiveSpell(true)`、`CastPointAreaHeal`、`ForEachPassiveAllyInRange` 并补齐 `CAbilityBlightedGoldMine` 通用矿产虚契约；将 `War3GameModule` 升级至 Version 18（新增运行时自定义技能自动施法 AI 策略注册接口 `RegisterAutocastAbility`）**。

## 1. 通用化引擎底座架构抽象（`src/engine/`）

为了将本项目从“单一硬编码的魔兽争霸3模拟器”演进为“可复用、可插拔的通用 RTS/RPG 现代游戏引擎”，Phase 12–18 抽离了零业务耦合的 `src/engine/` 引擎内核层，将底层服务与上层游戏规则（`IGameModule` + `ISimulationModule`）彻底解耦：

| 引擎底座模块 | 核心职责与设计模式 | 对标现代引擎参考 |
| ----- | ----- | ----- |
| `engine::ISubsystem` & `engine::SubsystemRegistry` (`src/engine/engine_subsystem.h`) | 定义 9 阶段确定性生命周期总线（`Platform(0) -> Jobs(50) -> Resource(100) -> Audio(200) -> Scripting(300) -> Simulation(400) -> Presentation(500) -> Render(600) -> UI(700) -> Telemetry(800)`），提供 `O(1)` 类型索引服务定位器（`Register<T>` / `TryGet<T>`）、按阶段升序初始化与逆序安全销毁 | Unity `SubsystemManager` / Godot `Server` 注册表 |
| `engine::FixedTimestepStepper` (`src/engine/fixed_timestep_stepper.h`) | 将固定频率确定性模拟步进（如 20Hz RTS 帧同步或 60Hz 物理步进）与高刷显示器渲染帧率（60/144/240Hz）彻底解耦；内置死亡螺旋钳制（`maxSubStepsPerFrame`）并自动计算 `[0, 1]` 帧间插值因子 `interpolationAlpha` | Godot `MainLoop` 物理插值分步器 / Unity `FixedUpdate` |
| `engine::IGameModule` (`src/engine/game_module.h`) | 定义通用引擎底座与具体游戏业务模块（如 `War3GameModule`、塔防/MOBA/RPG 模块或无头回归测试宿主）的标准生命周期契约（`OnRegisterSubsystems`、`OnInit`、`OnFixedTick`、`OnFrameUpdate`、`OnRender`、`OnShutdown`） | Godot `MainLoop` / Unity `World` 业务插件 |
| `engine::ISimulationModule` & `engine::NullSimulationModule` (`src/engine/simulation_module.h`) | **\[Phase 13–18 核心\]** 抽象 `CSimulation` 通用 ECS 世界容器、`CUnit` 通用实体、`CPlayer` 通用玩家状态、`CCombatSystem` 通用战斗计算器及 `MeleeAIController` 战术 AI 与具体游戏玩法规则的完整边界契约：涵盖技能目录挂载/卸载、单位原型技能装配与数据绑定（`OnAssembleUnitAbilities` / `OnUnitAbilityAdded`）、实体入世/阵亡/形态升级蜕变/退世钩子、科技树等效层级解析、科技升级实时属性加成引擎、经济维护费税率、胜负与残局判定、攻击/护甲克制倍率与护甲减伤曲线、战术 AI 托管与护甲克制目标评分、伤害前置/后置修正、命中被动触发、击杀赏金与英雄经验分配、空闲自动施法、物品属性路由、战役跨图英雄快照恢复、**\[Phase 17 新增\] 运行时声明式技能描述符动态注册契约（`RegisterCustomSpellDescriptor`）**，以及 **\[Phase 18 新增\] 运行时自定义技能自动施法 AI 策略动态注册契约（`RegisterAutocastAbility` / `GetAbilityAutocastKind`）**。挂载 `NullSimulationModule` 时 `CSimulation` 即刻进入 **Pure Engine Mode（纯净通用引擎模式）** | Godot `GDExtension` 玩法扩展 / Unity `Authoring & Rule System` |
| `engine::DynamicModuleLoader` (`src/engine/dynamic_module_loader.h/.cpp`) | **\[Phase 13 新增\]** 统一管理内置模块工厂（`RegisterBuiltInModule`）与操作系统原生共享库（`.so` / `.dll`）运行时动态加载（`LoadSharedLibrary`、`UnloadSharedLibrary`、`HotReloadSharedLibrary`），基于 POSIX `dlopen`/`dlsym`/`dlclose`（及 Windows `LoadLibraryA`/`GetProcAddress`）绑定 C ABI 导出入口，并通过 `shared_ptr` 自定义析构器安全托管跨 DLL 边界实例与代数（Generation）追踪 | Godot `GDExtensionLoader` / Unreal `FModuleManager` |
| `engine::EngineKernel` (`src/engine/engine_kernel.h/.cpp`) | 统一收拢七大核心引擎服务器（`JobSystem`、`ResourceServer`、`AudioServer`、`RenderServer + RenderGraph`、`InputActionMap`、`ScriptHostRouter`、`EngineProfiler`），支持**独立自治模式（`BootstrapStandardSubsystems`）**与**非侵入宿主绑定模式（`BindExternalServices`）**双轨运行 | Godot `OS + Engine` / Unity `PlayerLoop` |

## 2. Phase 13：`CSimulation` 单位/技能体系拆分为独立的 `War3GameModule` 动态模块

* **`CSimulation` 纯净化改造（`src/ecs/csimulation.h/.cpp`）**：移除全部 14 个具体魔兽技能 `#include`，将单位技能装配、物品/可破坏物配置、实体入世/退世结算统一委托给 `simModule_`。

* **模块作用域技能注册表（`src/ecs/ability_factory.h/.cpp`）**：`src/ecs/ability_factory.cpp` 从 **820+ 行** 精简至 **199 行**（累计缩减 **76%**），支持按 `moduleId` 注册、查询与批量卸载（`RegisterModuleCreator`、`RegisterModuleAlias`、`RegisterModuleSpellDescriptor`、`UnregisterModuleCreators`）。

* **原生共享库插件导出（`src/game/war3/war3_module_plugin_export.cpp` -> `libwar3_module_plugin.so`）**：导出标准 C ABI 入口符号，支持通过 `DynamicModuleLoader` 在运行时 `dlopen` 动态挂载或 `HotReloadSharedLibrary` 在线热重载。

## 3. Phase 14：将 `CPlayer` 科技树/升级体系与胜负判定规则抽离至 `War3GameModule` 插件

* **四族科技树等效推导与建筑升级蜕变同步（`War3GameModule::GetEffectiveTechLevel` & `OnUnitMorphed`）**：内置人族（`htow <- hkee <- hcas`）、兽族（`ogre <- ostr <- ofor`）、不死族（`unpl <- unp1 <- unp2`）、暗夜精灵（`etol <- etoa <- etoe`）及防御塔科技等效树，支持 `RegisterTechEquivalence` 动态扩展；建筑完成升级蜕变（`CAbilityUpgrade`）时自动转移科技计数与人口上限差值。

* **数据驱动的科技升级属性实时生效引擎（`CUpgradeEffectSpec` & `War3GameModule::ApplySingleUpgradeToUnit`）**：完整解析 `UpgradeData.slk` 的 `effect1..4`、`base1..4`、`mod1..4`、`code1..4` 与 `class`，支持护甲（`rarm`）、攻击力（`ratx`/`ratd`/`ratc`）、生命值（`rhpx`/`rhpo`）、法力值（`rmnx`/`rmno`）、移速（`rmvx`）、攻速（`rats`）与技能等级（`rlev`）对场上存活单位与新训练单位的增量幂等生效。

* **对战/同盟胜负判定与残局暴露规则引擎（`War3GameModule::EvaluateVictoryDefeat` & `IsPlayerCrippled`）**：封装无主基地且无农民/资金重建时的残局暴露（`Crippled`）、建筑全灭判负（`DEFEAT`）以及双向同盟共享胜利（`VICTORY`）规则与 `SimEventBus` 信号广播。

## 4. Phase 15：将 `CCombatSystem` 护甲/攻击克制表与 `MeleeAIController` 战术 AI 决策解耦至 `War3GameModule` 插件

* **`CCombatSystem` 护甲/攻击克制矩阵与减伤规则插件化（`src/ecs/combat/combat_system.h/.cpp`）**：

  * `War3GameModule::GetCanonicalWar3AttackDefenseTable()` 封装完整的魔兽3 TFT 7×8 克制矩阵，并实现 `GetAttackDefenseMultiplier`、`ComputeArmorDamageReduction`、`ShouldResistMagicDamage` 与 `EvaluateEtherealDamageFactor`。

  * 支持运行时通过 `War3GameModule::SetAttackDefenseMultiplier(atk, def, mult)`、`SetCombatTable(table)` 与 `SetDefenseArmorFactor(k)` 热修改克制倍率；切换至 `PureEngineMode` 时自动进入无克制偏置的 `1.0x` 纯净伤害模式。

* **`MeleeAIController` 战术 AI 生命周期托管与护甲克制集火评分（`src/ecs/melee_ai.h/.cpp`）**：

  * `War3GameModule` 统一托管 `aiControllers_` 生命周期、四族建造规划自动推导（`DiscoverRaceBuildPlan`）与护甲克制感知的集火目标效用评分（`EvaluateTacticalTargetScore`）。

## 5. Phase 16：`CUnit` 伤害反应/被动/赏金/自动施法全面插件化、`UnitStateFlag` 位域压缩与全库冗余清理

| 重构维度 | 具体改造内容 | 量化收益与通用引擎价值 |
| ----- | ----- | ----- |
| **`CUnit` 16 位紧凑状态位域（`UnitStateFlag : uint16_t`）** | 将 `CUnit` 上 16 个散落 `bool` 字段合并为单一 2 字节 `uint16_t stateFlags_` 位掩码，提供 `HasStateFlag`、`SetStateFlag`、`GetStateFlagsBitmask`、`SetStateFlagsBitmask`，并在 `SimStateSnapshot` 中支持单字位掩码无损快照与回滚恢复 | `CUnit` 状态标志内存占用从 **16 Bytes 降至 2 Bytes（缩减 87.5%）**，`O(1)` 批量位掩码序列化 |
| **`src/ecs/unit.cpp` 零具体技能头文件耦合** | 移除 `src/ecs/unit.cpp` 全部 **10 个** `ecs/abilities/...` 具体技能头文件；将技能数据表绑定、抗性皮肤减免、法术护盾拦截、伤害前置/后置修正、命中被动触发、击杀赏金与英雄经验分配、空闲自动施法全部通过 `ISimulationModule` 委托给 `War3GameModule` | `src/ecs/unit.cpp` 从 **2,017 行精简至 1,196 行（净减 821 行，缩减 40.7%）**；在 `PureEngineMode` 下 `CUnit` 成为纯粹的通用 ECS 实体 |
| **`War3GameModule` Version 16 战斗反应与赏金/经验引擎** | 在 `War3GameModule` 中完整实现 `OnEvaluatePreMitigationDamage`、`OnModifyPostMitigationDamage`、`OnPostDamageLanded`、`OnAwardKillBountyAndXP`、`OnTryUnitIdleAutocast`、`GetNegativeBuffDurationMultiplier`、`OnCheckSpellEffectReaction`，并新增运行时可热调的 `SetBountyMultiplier(mult)` 与 `SetExperienceMultiplier(mult)` 接口 | 支持自定义地图/模式动态调整全局赏金与经验倍率（如双倍经验/赏金模式），并可通过 `libwar3_module_plugin.so` (v16) 跨 `.so` 边界生效 |
| **物品加成与战役英雄跨图继承插件化（`item_bonuses.cpp` & `hero_carry.cpp`）** | 移除 `item_bonuses.cpp` 与 `hero_carry.cpp` 对 `ability_hero.h` / `ability_inventory.h` 的直接依赖，通过 `ISimulationModule::OnApplyUnitItemBonus`、`IsCarriableHero`、`OnSnapshotCarriedHero`、`OnRespawnCarriedHero` 路由（`hero_carry.cpp` 从 **112 行精简至 30 行，缩减 73.2%**） | 通用物品与跨场景实体迁移管线不再绑定魔兽3英雄三围（STR/AGI/INT）或背包槽位实现 |
| **数据层与工厂层死代码清理（`unitdata.cpp` & `ability_factory.cpp`）** | 删除 `CUnitData` 3 个废弃 ECS 耦合方法，移除 `unitdata.cpp` 对 `ecs/unit.h`、`ecs/csimulation.h`、`ecs/handle_id.h` 的反向依赖；删除 `AbilityFactory::AddQueueAbilities` 死函数并移除 `ability_queue.h` / `spells22.h` 耦合；清理 `game_controller.h`、`unit_text_utils.h`、`animation_utils.h` 头文件污染 | `src/data/` 成为零模拟层依赖的纯净数据解析层，全工程编译单元头文件展开体积显著降低 |

## 6. Phase 17：`src/ecs/` 与 `src/ecs/behaviors/*` 零具体技能头文件解耦与 `spells*.cpp` 声明式效果原语精简

| 重构维度 | 具体改造内容 | 量化收益与通用引擎价值 |
| ----- | ----- | ----- |
| **通用技能基类契约上提（`src/ecs/ability.h`）** | 将 `CAuraEffectType`、`CAuraBonus`、`CBuildRace` 上提至 `src/ecs/ability.h`，并在 `CAbility` 基类上定义施法能力（`GetCastRange`/`GetCastPoint`/`GetManaCost`/`StartCooldown`/`DoEffect`）、采集与返还（`GetCarriedResourceType`/`GetCarriedAmount`/`SetCarried`/`ClearCarried`/`GetLastTargetHandle`/`SetLastTargetHandle`/`IsCarryingResources`/`AcceptsResourceDropoff`）、矿产交互（`IsResourceMine`/`GetGold`/`SetGold`/`CanAddMiner`/`AddMiner`/`RemoveMiner`/`GetMiningDuration`）、建造/修理/工作状态（`GetBuildRace`/`GetRepairRate`/`GetRepairRange`/`IsActivelyWorking`）与物品栏（`PickUpItem`/`DropItem`/`DropItemAt`/`GetItem`/`FindItemSlot`）虚接口 | **`src/ecs/` 顶层与 `src/ecs/behaviors/` 对 `ecs/abilities/*` 的 `#include` 依赖从 14 处彻底清零至 0 处**；任何自定义游戏模块只需实现 `CAbility` 虚契约即可无缝驱动引擎的移动、建造、采集、修理、施法与拾取状态机 |
| **行为层与金矿技能去冗余合并（`behavior_return_resources.cpp` & `ability_gold_mine.cpp`）** | 提炼 `CBehaviorReturnResources::DepositCarriedResources` 消除重复交矿分支；在 `ability_gold_mine.cpp` 中提炼共享的 `TickGoldMineMiners` 与 `TryRegisterGoldMineMiner` 辅助内核，统一驱动普通金矿、闹鬼金矿与缠绕金矿 | 消除 3 类金矿与交矿状态机中的重复矿工结算循环，大幅降低维护成本 |
| **8 大声明式法术效果原语与组件合并（`ability_spell_base.h/.cpp` & `spells*.cpp`）** | 在 `CAbilitySpellBase` 中新增 `CastBuffOnTarget`、`CastBuffOnSelf`、`CastPointDamageWaves`、`CastCasterAreaDamageWaves`、`CastPointAreaBuff`、`CastCasterAreaBuff`、`CastLineDamageAndStun`、`CastTimedSpawnerUnit` 8 个声明式原语；新增共享 `CAttackBonusDamageEffect` 统一驱动 `AIfb`/`AHfa`/`AIcb` 等攻击附伤技能；将 `spells25.h/.cpp` 中 4 个重复的物品属性类合并为参数化的 `CAbilityItemStatBonus(StatSlot)`（统一驱动 `AIat`/`AIde`/`AIml`/`AImm` 及 `AIab`） | 重构精简 `spells.cpp`、`spells2.cpp`、`spells4.cpp`、`spells5.cpp`、`spells7.cpp`..`spells14.cpp`、`spells16.cpp`..`spells20.cpp`、`spells25.cpp` 共 **18 个法术文件**，消除数百行重复样板代码 |
| **`War3GameModule` Version 17 动态声明式法术注册（`RegisterCustomSpellDescriptor`）** | 将 `War3GameModule` 与 `libwar3_module_plugin.so` 升级至 **Version `17u`**，实现 `ISimulationModule::RegisterCustomSpellDescriptor`，支持在运行时无需编写任何 C++ 子类即可向模块动态注册数据驱动的 `ABSpellDescriptor` 法术，并在模块卸载时随模块自动清理 | 自定义地图/Mod 开发者可通过声明式动作（`ABAction*` / `ABEffectChain`）动态扩展新技能 |

## 7. Phase 18：`CBuff`/`CAbility` 位域内存布局压缩、`CBuffCloneable` CRTP 深拷贝模板与 `War3GameModule` v18 自定义自动施法

### 7.1 诊断与解耦/精简目标

在完成 Phase 17 后，对 `src/ecs/buffs.h/.cpp`、`src/ecs/ability.h`、`src/ecs/abilities/ability_spell_base.h/.cpp` 以及后期技能批次 `spells6.cpp`、`spells8.cpp`、`spells21.cpp`、`spells26.cpp`..`spells29.cpp` 的深度审查发现了四个可进一步提炼现代化的核心维度：

1. **高频 ECS 组件（`CBuff`、`CAbility`、`CAbilitySpellBase`）内存布局存在对齐空洞与字段重复**：
   * `CAbilitySpellBase` 重复定义了 `int level_ = 1`，遮蔽（shadow）了基类 `CAbility::level_`；同时 `CAbility` 与 `CBuff` 上散落多个独立 `bool` 字段，在 64 位架构下因结构体内对齐填充浪费了 20%–28.6% 的缓存行空间。
2. **`src/ecs/buffs.h` 与 `src/ecs/buffs.cpp` 存在 68 处手写 `Clone()` 样板与状态覆盖 Bug**：
   * 68 个 `CBuff` 派生类各自手写 `CBuff* Clone() const override`，且 `buffs.cpp` 中 30 余个无状态/单字段 Buff 的构造、析构与状态恢复代码高度重复；
   * 当单位同时拥有多个赋予相同状态（如 `DivineShield` + `StoneForm` 同时赋予无敌，或 `StoneForm` + `Ensnare` 同时赋予移动定身，或 `AntiMagicShell` + `Whirlwind` 同时赋予魔免）的 Buff 时，先到期的 Buff 在 `OnRemove` 中直接调用 `target->SetInvulnerable(false)` / `SetRooted(false)` / `SetMagicImmune(false)`，错误清除了仍在生效的另一个 Buff 的状态。
3. **被动法术禁用目标校验与区域治疗/友军遍历样板重复（`spells6`、`spells8`、`spells21`、`spells26`..`spells29`）**：
   * `spells26.h/.cpp`、`spells27.h/.cpp`、`spells29.h/.cpp` 中十余个被动技能各自重写 `CheckCanTarget`、`CheckCanTargetPoint`、`CheckCanTargetNoTarget` 三个返回 `false` 的虚函数；
   * 多处法术重复手写“遍历施法点半径内友军执行治疗或状态赋予”的几何距离循环；
   * `spells28.h` 中的 `CAbilityBlightedGoldMine` 尚未接入 `CAbility` 通用矿产虚契约（`IsResourceMine`/`GetGold`/`SetGold`/`CanAddMiner`/`IsActivelyWorking`）。
4. **自定义法术缺少运行时自动施法（Autocast）AI 策略注册通道**：
   * Phase 17 支持了运行时动态注册 `ABSpellDescriptor` 自定义法术，但 `War3GameModule` 的空闲自动施法分派器（`AutocastKindForAbility`）仍仅识别硬编码的内置法术 ID，无法为动态注册的自定义法术配置自动施法策略。

### 7.2 Phase 18 核心架构改造与精简成果

| 重构维度 | 具体改造内容 | 量化收益与通用引擎价值 |
| ----- | ----- | ----- |
| **`CBuff` / `CAbility` / `CAbilitySpellBase` 位域紧凑内存布局** | 消除 `CAbilitySpellBase::level_` 遮蔽字段；将 `CAbility` 的 `level_` 压缩为 `int16_t` 并将 5 个布尔标志（`physical_`、`permanent_`、`iconShowing_`、`autocastActive_`、`passiveSpell_`）打包为 1-bit 位域（支持 `AbilityStateFlag` 位掩码 O(1) 快照序列化）；将 `CBuff` 的 8 个布尔标志（`expired_`、`suspended_`、`grantsInvulnerable_`、`grantsRooted_`、`grantsInvisible_`、`grantsMagicImmune_` 等）打包为紧凑位域（`BuffStateFlag`） | **`sizeof(CBuff)` 从 56B 降至 40B（缩减 28.6%）**；**`sizeof(CAbility)` 从 40B 降至 32B（缩减 20%）**；**`sizeof(CAbilitySpellBase)` 从 72B 降至 56B（缩减 22.2%）**；显著提升大规模团战遍历時の CPU L1/L2 缓存命中率 |
| **CRTP 零样板深拷贝模板（`CBuffCloneable<Derived, Base>`）** | 在 `src/ecs/buffs.h` 中引入支持单层与多层继承（如 `CBuffPolymorph : public CBuffCloneable<CBuffPolymorph, CBuffHex>`、`CBuffCyclone : public CBuffCloneable<CBuffCyclone, CBuffStun>`）的 CRTP 模板类，自动通过拷贝构造完成深拷贝 | 一次性消除 **68 个** Buff 派生类中手写的 `Clone()` 虚函数声明与实现；`src/ecs/buffs.cpp` 从 **1,111 行精简至 774 行（净减 337 行，缩减 30.3%）** |
| **多 Buff 叠加状态安全恢复与周期 AoE 伤害共享内核** | 在 `CBuff` 基类引入声明式状态授予标记（`grantsInvulnerable_`、`grantsRooted_`、`grantsInvisible_`、`grantsMagicImmune_`）与 `RestoreSharedUnitStateFlags` 引用计数扫描：当某个 Buff 移除时，自动检查单位身上是否仍有其他未过期 Buff 授予同类状态，仅在最后一个同类 Buff 消失时才复位单位状态；同时提炼 `CBuff::DamageEnemyUnitsInRadius` 统一驱动 `CBuffPhoenixFire`、`CBuffWhirlwind`、`CBuffPermanentImmolation`、`CBuffLiquidFire`、`CBuffExplode` 等周期/爆炸 AoE | 彻底修复多重无敌/定身/隐身/魔免 Buff 叠加时的早退清除 Bug，并消除 6 个周期 AoE Buff 的重复敌方过滤与伤害循环 |
| **被动技能标志（`SetPassiveSpell`）、区域治疗原语与不死族金矿虚契约（`spells6`..`spells29`）** | 在 `CAbilitySpellBase` 中支持 `SetPassiveSpell(true)` 自动拦截 `CheckCanTarget*` 主动施法校验，删除 `spells25`、`spells26`、`spells27`、`spells29` 中 **45+ 个** 冗余 `CheckCanTarget*` 空实现；新增 `CastPointAreaHeal` 与 `ForEachPassiveAllyInRange` 原语精简 `spells6`、`spells8`、`spells21`、`spells26`、`spells27`；在 `CAbilityBlightedGoldMine`（`spells28.h`）上实现完整的 `CAbility` 矿产虚契约 | `spells26..29` 与 `buffs` 合计净减 **500+ 行** 重复样板代码；不死族闹鬼金矿可被通用引擎接口透明查询与操控 |
| **`War3GameModule` Version 18 自定义自动施法注册（`RegisterAutocastAbility`）** | 将 `War3GameModule` 与 `libwar3_module_plugin.so` 升级至 **Version `18u`**，在 `ISimulationModule` 与 `War3GameModule` 中新增 `RegisterAutocastAbility(abilityCode, autocastKind)` 与 `GetAbilityAutocastKind(rawcode)`，支持为运行时动态注册的自定义法术绑定 AI 自动施法策略（如 `LOWEST_HP_ALLY`、`ALLY_COMBAT_BUFF`、`ENEMY_COMBAT_DEBUFF` 等），并在模块卸载时自动清理 | 自定义数据驱动技能不仅能手动施放，还可无缝接入单位空闲自动施法 AI 管线 |

## 8. 云端游戏资源验证与单元测试覆盖

结合云端 Google Drive [`WarCraft3`](https://drive.google.com/drive/folders/1sEyljyjTcnwtxQNPEhYC2dVMh4Idit-L) / [`Maps/FrozenThrone`](https://drive.google.com/drive/folders/1sb1fktbnjqbIEjNrKky4l_iWyMfb_wU3) 下的 14 张零售级对战与 [`Scenario`](https://drive.google.com/drive/folders/18rVJURpX4ZHWLumjNUp0ZeXEN-TJOaj-) 地图（含 [`(2)EchoIsles.w3x`](https://drive.google.com/file/d/1d5qijwX3yNOvm9Xej64YpfSTivPlNGff/view?usp=drivesdk)、[`(4)TurtleRock.w3x`](https://drive.google.com/file/d/106f_pFaSePSYhTJTZezXX4NYGotoG1pY/view?usp=drivesdk)、[`(4)TwistedMeadows.w3x`](https://drive.google.com/file/d/1v51wDc9ilk5BtPbEsPUX0mZVWnA3olix/view?usp=drivesdk)、[`(8)GardenOfWar.w3x`](https://drive.google.com/file/d/1LOvXHMDAyjLaLdZlAg4ubMIAcqNi6qrP/view?usp=drivesdk)、[`(6)BlizzardTD.w3x`](https://drive.google.com/file/d/1NK4On-ccrEEzFzRPsTv625yZY5QXl-h8/view?usp=drivesdk)、[`(8)AzerothGrandPrix.w3x`](https://drive.google.com/file/d/1H-h7ZYHa2qdwykkdNkpSaRDdKuNojLAM/view?usp=drivesdk) 等），全量运行 **841 项** 单元与集成测试：

* **Phase 12–16 专项测试（`Stage EE1`–`Stage OO2`）**：20 项全通过。

* **Phase 17 专项测试**：

  * `Stage PP1`: 验证 `src/ecs/` 与 `src/ecs/behaviors/*` 零具体技能头文件耦合，并通过 `CAbility` 通用虚契约端到端验证矿工进矿采集（`IsResourceMine` / `AddMiner` / `IsActivelyWorking`）、黄金携带（`IsCarryingResources` / `GetUnitCarriedResourceType`）与基地交矿（`AcceptsResourceDropoff`）。

  * `Stage PP2`: 通过 `CAbility` 通用虚契约验证施法（`GetCastRange`/`GetCastPoint`/`DoEffect`）、工兵修理（`GetRepairRate`/`GetRepairRange`）、英雄背包拾取与丢弃（`PickUpItem`/`GetItem`/`FindItemSlot`/`DropItem`），以及 `CAbilityQueue::IsActivelyWorking` 训练队列工作态标签推导。

  * `Stage QQ1`: 验证 `CAbilitySpellBase` 声明式效果原语（`CastBuffOnTarget` 妖术变形、`CastBuffOnSelf` 狂暴自增益、`CastCasterAreaBuff` 战争践踏眩晕、`CastTimedSpawnerUnit` 炎魔召唤）、共享攻击附伤组件 `CAttackBonusDamageEffect`（`AIfb`/`AHfa`）以及统一物品属性加成组件 `CAbilityItemStatBonus`（`AIat`/`AIde`/`AIml`/`AImm`）。

  * `Stage QQ2`: 验证 `War3GameModule` Version 17（及动态加载 `./libwar3_module_plugin.so`）运行时通过 `RegisterCustomSpellDescriptor` 动态注册无 C++ 子类的自定义法术（`ACp7`）并完整执行施法与伤害结算，以及云端零售地图 `(2)EchoIsles.w3x` 与 `(4)TwistedMeadows.w3x` 端到端加载验证。

* **Phase 18 专项测试**：

  * `Stage RR1`: 验证 `CAbility`（`<= 32B`）、`CBuff`（`<= 40B`）、`CAbilitySpellBase`（`<= 56B`）位域压缩内存布局，`AbilityStateFlag` 位掩码无损往返序列化，以及 `CBuffCloneable` 单层（`CBuffSlow`）与多层继承（`CBuffPolymorph -> CBuffHex`、`CBuffCyclone -> CBuffStun`）CRTP 深拷贝保真度。

  * `Stage RR2`: 验证多重同状态 Buff 叠加（`CBuffDivineShield` + `CBuffStoneForm` 无敌叠加、`CBuffStoneForm` + `CBuffEnsnare` 定身叠加、`CBuffMagicImmune` + `CBuffWhirlwind` 魔免叠加）移除时的引用计数状态保持，以及 `CBuff::DamageEnemyUnitsInRadius` 敌我识别 AoE 伤害结算。

  * `Stage SS1`: 验证 `SetPassiveSpell(true)` 被动技能目标校验拦截（`CAbilityItemSpellResist`、`CAbilityItemMixedDefense`、`CAbilityChaos`）、`CastPointAreaHeal` 区域友军治疗（`CAbilityHealingSpray`）、`ForEachPassiveAllyInRange` 范围友军护盾赋予（`CAbilitySpellShieldArea`），以及 `CAbilityBlightedGoldMine` 通用矿产虚契约。

  * `Stage SS2`: 验证 `War3GameModule` Version 18（及动态加载 `./libwar3_module_plugin.so` v18）运行时组合使用 `RegisterCustomSpellDescriptor` 与 `RegisterAutocastAbility` 注册自定义自动治疗法术（`ACh8`），由空闲单位自动向受伤友军施放治疗，并通过云端零售地图 `(2)EchoIsles.w3x` 与 `(4)TurtleRock.w3x` 端到端加载验证。

* **总计 841 / 841 项测试全部通过（100% 通过率，零回归）**。
