# 现代化游戏引擎底座抽象与精简重构设计文档 (Phase 12 – Phase 16)

本项目以 **WarCraft III (Warsmash C++)** 为实验载体，参考 **Godot 4**（`OS / MainLoop`、Server 单例架构、`GDExtension` 动态插件系统、`SceneTree`、`ResourceServer`）与 **Unity**（`PlayerLoop`、`SubsystemManager`、`DOTS Sparse-Set ECS`、可热插拔 `GamePlay Assembly`、数据驱动 `ScriptableObject`）的设计哲学，在已完成 Phase 1–11 现代化改造的基础上，进一步完成：

* **Phase 12**：**通用游戏引擎底座（`engine::*`）抽象解耦** 与 **全库去冗余/过时代码精简（De-Redundancy & Legacy Pruning）**。

* **Phase 13**：**将 `CSimulation` 内的魔兽争霸3单位/技能体系彻底拆分为独立的 `war3::War3GameModule` 动态可插拔模块（含 `engine::ISimulationModule` 模拟策略接口、模块作用域技能目录管理与 OS 原生 `.so`/`.dll` `dlopen` 动态热重载器 `engine::DynamicModuleLoader`）**。

* **Phase 14**：**继续将 `CPlayer` 科技树/升级体系（Techtree Tier Equivalences & Live Upgrade Effect Engine）、维护费税率（Upkeep Tax）与对战/同盟胜负及残局暴露判定规则（Melee & Allied Victory/Defeat & Crippled Rules）进一步抽离至 `war3::War3GameModule` 动态插件中**。

* **Phase 15**：**继续将 `CCombatSystem` 7×8 护甲/攻击类型克制表、0.06 护甲减伤曲线、虚无/魔免规则，以及 `MeleeAIController` 战术 AI 生命周期托管、四族建造规划推导与护甲克制优先集火评分（Armor-Counter Tactical Target Scoring）全面解耦至 `war3::War3GameModule` 动态插件中**。

* **Phase 16**：**完成 `CUnit`、数据层 `CUnitData`、物品属性管线 `item_bonuses`、战役跨图英雄继承 `hero_carry` 与核心头文件体系的全面去耦合与精简：彻底移除 `src/ecs/unit.cpp` 内全部 10 个具体魔兽技能头文件（`unit.cpp` 代码量从 2,017 行精简至 1,196 行，缩减 40.7%），将 `CUnit` 16 个散落布尔字段压缩为 2 字节 `uint16_t` 位域标志（`UnitStateFlag`，内存占用缩减 87.5%），清理 `src/data/unitdata.cpp` 残留的 ECS 越层死代码，并将伤害前置/后置反应、受击被动触发、击杀赏金骰子、英雄经验分配、空闲自动施法、物品三维属性加成与战役英雄跨图快照全面收归 `war3::War3GameModule` (Version 16)**。

## 1. 通用化引擎底座架构抽象（`src/engine/`）

为了将本项目从“单一硬编码的魔兽争霸3模拟器”演进为“可复用、可插拔的通用 RTS/RPG 现代游戏引擎”，Phase 12–16 抽离了零业务耦合的 `src/engine/` 引擎内核层，将底层服务与上层游戏规则（`IGameModule` + `ISimulationModule`）彻底解耦：

| 引擎底座模块 | 核心职责与设计模式 | 对标现代引擎参考 | 
| ----- | ----- | ----- | 
| `engine::ISubsystem` & `engine::SubsystemRegistry` (`src/engine/engine_subsystem.h`) | 定义 9 阶段确定性生命周期总线（`Platform(0) -> Jobs(50) -> Resource(100) -> Audio(200) -> Scripting(300) -> Simulation(400) -> Presentation(500) -> Render(600) -> UI(700) -> Telemetry(800)`），提供 `O(1)` 类型索引服务定位器（`Register<T>` / `TryGet<T>`）、按阶段升序初始化与逆序安全销毁 | Unity `SubsystemManager` / Godot `Server` 注册表 | 
| `engine::FixedTimestepStepper` (`src/engine/fixed_timestep_stepper.h`) | 将固定频率确定性模拟步进（如 20Hz RTS 帧同步或 60Hz 物理步进）与高刷显示器渲染帧率（60/144/240Hz）彻底解耦；内置死亡螺旋钳制（`maxSubStepsPerFrame`）并自动计算 `[0, 1]` 帧间插值因子 `interpolationAlpha` | Godot `MainLoop` 物理插值分步器 / Unity `FixedUpdate` | 
| `engine::IGameModule` (`src/engine/game_module.h`) | 定义通用引擎底座与具体游戏业务模块（如 `War3GameModule`、塔防/MOBA/RPG 模块或无头回归测试宿主）的标准生命周期契约（`OnRegisterSubsystems`、`OnInit`、`OnFixedTick`、`OnFrameUpdate`、`OnRender`、`OnShutdown`） | Godot `MainLoop` / Unity `World` 业务插件 | 
| `engine::ISimulationModule` & `engine::NullSimulationModule` (`src/engine/simulation_module.h`) | **\[Phase 13–16 核心\]** 抽象 `CSimulation` 通用 ECS 世界容器、`CUnit` 通用实体、`CPlayer` 通用玩家状态、`CCombatSystem` 通用战斗计算器及 `MeleeAIController` 战术 AI 与具体游戏玩法规则的完整边界契约：涵盖技能目录挂载/卸载、单位原型技能装配与数据绑定（`OnAssembleUnitAbilities` / `OnUnitAbilityAdded`）、实体入世/阵亡/形态升级蜕变/退世钩子、科技树等效层级解析、科技升级实时属性加成引擎、经济维护费税率、胜负与残局判定、攻击/护甲克制倍率与护甲减伤曲线、战术 AI 托管与护甲克制目标评分，以及 **\[Phase 16 新增\] 负面状态抗性皮肤折算（`GetNegativeBuffDurationMultiplier`）、法术护盾拦截反应（`OnCheckSpellEffectReaction`）、伤害前置吸收/削弱/免疫判定（`OnEvaluatePreMitigationDamage`）、伤害后置防御/狂暴/符文护腕修正（`OnModifyPostMitigationDamage`）、命中后被动触发管线（`OnPostDamageLanded`）、击杀赏金骰子与英雄经验分配（`OnAwardKillBountyAndXP`）、空闲自动施法扫描（`OnTryUnitIdleAutocast`）、物品属性加成路由（`OnApplyUnitItemBonus`）与战役跨图英雄快照恢复（`IsCarriableHero` / `OnSnapshotCarriedHero` / `OnRespawnCarriedHero`）**。挂载 `NullSimulationModule` 时 `CSimulation` 即刻进入 **Pure Engine Mode（纯净通用引擎模式）** | Godot `GDExtension` 玩法扩展 / Unity `Authoring & Rule System` | 
| `engine::DynamicModuleLoader` (`src/engine/dynamic_module_loader.h/.cpp`) | **\[Phase 13 新增\]** 统一管理内置模块工厂（`RegisterBuiltInModule`）与操作系统原生共享库（`.so` / `.dll`）运行时动态加载（`LoadSharedLibrary`、`UnloadSharedLibrary`、`HotReloadSharedLibrary`），基于 POSIX `dlopen`/`dlsym`/`dlclose`（及 Windows `LoadLibraryA`/`GetProcAddress`）绑定 C ABI 导出入口，并通过 `shared_ptr` 自定义析构器安全托管跨 DLL 边界实例与代数（Generation）追踪 | Godot `GDExtensionLoader` / Unreal `FModuleManager` | 
| `engine::EngineKernel` (`src/engine/engine_kernel.h/.cpp`) | 统一收拢七大核心引擎服务器（`JobSystem`、`ResourceServer`、`AudioServer`、`RenderServer + RenderGraph`、`InputActionMap`、`ScriptHostRouter`、`EngineProfiler`），支持\*\*独立自治模式（`BootstrapStandardSubsystems`）**与**非侵入宿主绑定模式（`BindExternalServices`）\*\*双轨运行 | Godot `OS + Engine` / Unity `PlayerLoop` | 

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

### 5.1 诊断与解耦目标

在完成 Phase 12–15 后，通用 ECS 实体层与数据层仍残留以下魔兽3专属耦合与冗余设计：

1. **`src/ecs/unit.cpp` 严重依赖具体魔兽技能与状态类**：`unit.cpp` 顶部直接包含了 10 个具体技能头文件（`ability_spell_base.h`、`ability_hero.h`、`ability_transport.h`、`spells4.h`、`spells5.h`、`spells9.h`、`spells10.h`、`spells14.h`、`spells16.h`、`spells28.h`），并在 `CUnit::Damage`、`CUnit::Update`、`CUnit::AddAbility`、`CUnit::AddBuff` 中硬编码了魔法护盾（`BNms`）、恐怖嚎叫（`BNht`）、天神下凡（`BAvt`）、防御（`CBuffDefend`）、狂暴（`CBuffBerserk`）、荆棘光环/尖刺外壳反伤（`AUts`）、吸血光环（`CBuffLifesteal`）、重击眩晕（`AHbh`）、分裂攻击（`ANca`）、疾风步破隐背刺（`BOwk`）、粉碎（`Awar`）、抗性皮肤（`Brsk`）、法术护盾（`BNss`）、击杀赏金骰子与英雄经验分配公式，以及 8 种空闲自动施法逻辑。

2. **`CUnit` 实例布尔标志内存碎片**：`CUnit` 内部散落了 16 个独立的 `bool` 成员变量（`building_`、`hero_`、`hidden_`、`paused_`、`invulnerable_`、`rooted_`、`invisible_`、`detector_`、`ethereal_`、`magicImmune_`、`isIllusion_`、`useFood_`、`isPathingEnabled_`、`canSleep_`、`canSleepPerm_`、`ignoreAlarm_`），占用 16 字节且快照序列化繁琐。

3. **`item_bonuses.cpp`、`hero_carry.cpp`、`ability_factory.cpp`、`unitdata.cpp` 与核心头文件的跨层耦合与死代码**：

   * `src/ecs/item_bonuses.cpp` 与 `src/ecs/hero_carry.cpp` 直接包含 `ability_hero.h` / `ability_inventory.h`；

   * `src/ecs/ability_factory.cpp` 仍包含 `ability_queue.h` 与 `spells22.h`，且残留从未被调用的 `AddQueueAbilities` 死函数；

   * `src/data/unitdata.h/.cpp` 残留 Phase 13 之前的 3 个废弃方法（`CreateUnit`、`AddDefaultAbilitiesToUnit`、`ApplyPlayerUpgradesToUnit`），导致纯数据层 `src/data/` 反向依赖 `ecs/unit.h` 与 `ecs/csimulation.h`；

   * `src/core/game_controller.h`、`src/ecs/unit_text_utils.h`、`src/ecs/animation_utils.h` 包含了不必要的具体技能头文件（`ability_gold_mine.h`、`ability_queue.h`、`ability_harvest.h`）。

### 5.2 Phase 16 核心架构改造与精简成果

| 重构维度 | 具体改造内容 | 量化收益与通用引擎价值 | 
| ----- | ----- | ----- | 
| **`CUnit` 16 位紧凑状态位域（`UnitStateFlag : uint16_t`）** | 将 `CUnit` 上 16 个散落 `bool` 字段合并为单一 2 字节 `uint16_t stateFlags_` 位掩码，提供 `HasStateFlag`、`SetStateFlag`、`GetStateFlagsBitmask`、`SetStateFlagsBitmask`，并在 `SimStateSnapshot` 中支持单字位掩码无损快照与回滚恢复 | `CUnit` 状态标志内存占用从 **16 Bytes 降至 2 Bytes（缩减 87.5%）**，`O(1)` 批量位掩码序列化 | 
| **`src/ecs/unit.cpp` 零具体技能头文件耦合** | 移除 `src/ecs/unit.cpp` 全部 **10 个** `ecs/abilities/...` 具体技能头文件；将技能数据表绑定、抗性皮肤减免、法术护盾拦截、伤害前置/后置修正、命中被动触发、击杀赏金与英雄经验分配、空闲自动施法全部通过 `ISimulationModule` 委托给 `War3GameModule` | `src/ecs/unit.cpp` 从 **2,017 行精简至 1,196 行（净减 821 行，缩减 40.7%）**；在 `PureEngineMode` 下 `CUnit` 成为纯粹的通用 ECS 实体 | 
| **`War3GameModule` Version 16 战斗反应与赏金/经验引擎** | 在 `War3GameModule` 中完整实现 `OnEvaluatePreMitigationDamage`、`OnModifyPostMitigationDamage`、`OnPostDamageLanded`、`OnAwardKillBountyAndXP`、`OnTryUnitIdleAutocast`、`GetNegativeBuffDurationMultiplier`、`OnCheckSpellEffectReaction`，并新增运行时可热调的 `SetBountyMultiplier(mult)` 与 `SetExperienceMultiplier(mult)` 接口 | 支持自定义地图/模式动态调整全局赏金与经验倍率（如双倍经验/赏金模式），并可通过 `libwar3_module_plugin.so` (v16) 跨 `.so` 边界生效 | 
| **物品加成与战役英雄跨图继承插件化（`item_bonuses.cpp` & `hero_carry.cpp`）** | 移除 `item_bonuses.cpp` 与 `hero_carry.cpp` 对 `ability_hero.h` / `ability_inventory.h` 的直接依赖，通过 `ISimulationModule::OnApplyUnitItemBonus`、`IsCarriableHero`、`OnSnapshotCarriedHero`、`OnRespawnCarriedHero` 路由（`hero_carry.cpp` 从 **112 行精简至 30 行，缩减 73.2%**） | 通用物品与跨场景实体迁移管线不再绑定魔兽3英雄三围（STR/AGI/INT）或背包槽位实现 | 
| **数据层与工厂层死代码清理（`unitdata.cpp` & `ability_factory.cpp`）** | 删除 `CUnitData` 3 个废弃 ECS 耦合方法，移除 `unitdata.cpp` 对 `ecs/unit.h`、`ecs/csimulation.h`、`ecs/handle_id.h` 的反向依赖；删除 `AbilityFactory::AddQueueAbilities` 死函数并移除 `ability_queue.h` / `spells22.h` 耦合；清理 `game_controller.h`、`unit_text_utils.h`、`animation_utils.h` 头文件污染 | `src/data/` 成为零模拟层依赖的纯净数据解析层，全工程编译单元头文件展开体积显著降低 | 

## 6. 云端游戏资源验证与单元测试覆盖

结合云端 Google Drive [`WarCraft3`](https://drive.google.com/drive/folders/1sEyljyjTcnwtxQNPEhYC2dVMh4Idit-L) / [`Maps/FrozenThrone`](https://drive.google.com/drive/folders/1sb1fktbnjqbIEjNrKky4l_iWyMfb_wU3) 下的 14 张零售级对战与 [`Scenario`](https://drive.google.com/drive/folders/18rVJURpX4ZHWLumjNUp0ZeXEN-TJOaj-) 地图（含 [`(2)EchoIsles.w3x`](https://drive.google.com/file/d/1d5qijwX3yNOvm9Xej64YpfSTivPlNGff/view?usp=drivesdk)、[`(4)TurtleRock.w3x`](https://drive.google.com/file/d/106f_pFaSePSYhTJTZezXX4NYGotoG1pY/view?usp=drivesdk)、[`(4)TwistedMeadows.w3x`](https://drive.google.com/file/d/1v51wDc9ilk5BtPbEsPUX0mZVWnA3olix/view?usp=drivesdk)、[`(8)GardenOfWar.w3x`](https://drive.google.com/file/d/1LOvXHMDAyjLaLdZlAg4ubMIAcqNi6qrP/view?usp=drivesdk)、[`(6)BlizzardTD.w3x`](https://drive.google.com/file/d/1NK4On-ccrEEzFzRPsTv625yZY5QXl-h8/view?usp=drivesdk)、[`(8)AzerothGrandPrix.w3x`](https://drive.google.com/file/d/1H-h7ZYHa2qdwykkdNkpSaRDdKuNojLAM/view?usp=drivesdk) 等），全量运行 **833 项** 单元与集成测试：

* **Phase 12 专项测试（`Stage EE1`–`Stage GG1`）**：4 项全通过。

* **Phase 13 专项测试（`Stage HH1`–`Stage II2`）**：4 项全通过。

* **Phase 14 专项测试（`Stage JJ1`–`Stage KK2`）**：4 项全通过。

* **Phase 15 专项测试（`Stage LL1`–`Stage MM2`）**：4 项全通过。

* **Phase 16 专项测试**：

  * `Stage NN1`: `CUnit` 16 位 `UnitStateFlag` 位域操作、`SimStateSnapshot` 位掩码无损快照恢复、抗性皮肤（`Brsk` 25% 负面状态时长折算）与法术护盾（`BNss` 敌方法术拦截）在 `War3GameModule` 与 `PureEngineMode` 下的对照验证。

  * `Stage NN2`: `War3GameModule` 伤害前置修正（魔法护盾 `BNms` 耗蓝抵伤、恐怖嚎叫 `BNht` 攻击削弱）、后置修正（防御 `CBuffDefend` 50% 武器减伤）与受击被动触发（吸血光环 `CBuffLifesteal`、荆棘反伤 `CBuffThorns`、分裂攻击 `ANca`、疾风步 `BOwk` 破隐背刺）与 `PureEngineMode` 纯净引擎模式对照验证。

  * `Stage OO1`: `War3GameModule` 英雄可学技能列表装配、自定义 `2.0x` 击杀赏金骰子与英雄经验分配倍率（`SetBountyMultiplier` / `SetExperienceMultiplier`）、物品力量属性（`+10 STR -> +250 HP`）路由，以及战役英雄跨图快照恢复（`SnapshotHero` / `RespawnCarriedHero`）验证。

  * `Stage OO2`: 动态插件 `./libwar3_module_plugin.so`（Version 16）运行时 `dlopen` 加载、跨 `.so` 边界击杀赏金结算，以及云端零售地图 `(2)EchoIsles.w3x` 与 `(4)TurtleRock.w3x` 端到端加载与 `SimStateSnapshot` 位域快照恢复验证。

* **总计 833 / 833 项测试全部通过（100% 通过率，零回归）**。