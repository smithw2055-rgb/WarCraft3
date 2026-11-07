# 现代化游戏引擎底座抽象与精简重构设计文档 (Phase 12 – Phase 15)

本项目以 **WarCraft III (Warsmash C++)** 为实验载体，参考 **Godot 4**（`OS / MainLoop`、Server 单例架构、`GDExtension` 动态插件系统、`SceneTree`、`ResourceServer`）与 **Unity**（`PlayerLoop`、`SubsystemManager`、`DOTS Sparse-Set ECS`、可热插拔 `GamePlay Assembly`、数据驱动 `ScriptableObject`）的设计哲学，在已完成 Phase 1–11 现代化改造的基础上，进一步完成：

* **Phase 12**：**通用游戏引擎底座（`engine::*`）抽象解耦** 与 **全库去冗余/过时代码精简（De-Redundancy & Legacy Pruning）**。

* **Phase 13**：**将 `CSimulation` 内的魔兽争霸3单位/技能体系彻底拆分为独立的 `war3::War3GameModule` 动态可插拔模块（含 `engine::ISimulationModule` 模拟策略接口、模块作用域技能目录管理与 OS 原生 `.so`/`.dll` `dlopen` 动态热重载器 `engine::DynamicModuleLoader`）**。

* **Phase 14**：**继续将 `CPlayer` 科技树/升级体系（Techtree Tier Equivalences & Live Upgrade Effect Engine）、维护费税率（Upkeep Tax）与对战/同盟胜负及残局暴露判定规则（Melee & Allied Victory/Defeat & Crippled Rules）进一步抽离至 `war3::War3GameModule` 动态插件中**。

* **Phase 15**：**继续将 `CCombatSystem` 7×8 护甲/攻击类型克制表、0.06 护甲减伤曲线、虚无/魔免规则，以及 `MeleeAIController` 战术 AI 生命周期托管、四族建造规划推导与护甲克制优先集火评分（Armor-Counter Tactical Target Scoring）全面解耦至 `war3::War3GameModule` 动态插件中**。

## 1. 通用化引擎底座架构抽象（`src/engine/`）

为了将本项目从“单一硬编码的魔兽争霸3模拟器”演进为“可复用、可插拔的通用 RTS/RPG 现代游戏引擎”，Phase 12–15 抽离了零业务耦合的 `src/engine/` 引擎内核层，将底层服务与上层游戏规则（`IGameModule` + `ISimulationModule`）彻底解耦：

| 引擎底座模块 | 核心职责与设计模式 | 对标现代引擎参考 |
| :--- | :--- | :--- |
| `engine::ISubsystem` & `engine::SubsystemRegistry` (`src/engine/engine_subsystem.h`) | 定义 9 阶段确定性生命周期总线（`Platform(0) -> Jobs(50) -> Resource(100) -> Audio(200) -> Scripting(300) -> Simulation(400) -> Presentation(500) -> Render(600) -> UI(700) -> Telemetry(800)`），提供 `O(1)` 类型索引服务定位器（`Register<T>` / `TryGet<T>`）、按阶段升序初始化与逆序安全销毁 | Unity `SubsystemManager` / Godot `Server` 注册表 |
| `engine::FixedTimestepStepper` (`src/engine/fixed_timestep_stepper.h`) | 将固定频率确定性模拟步进（如 20Hz RTS 帧同步或 60Hz 物理步进）与高刷显示器渲染帧率（60/144/240Hz）彻底解耦；内置死亡螺旋钳制（`maxSubStepsPerFrame`）并自动计算 `[0, 1]` 帧间插值因子 `interpolationAlpha` | Godot `MainLoop` 物理插值分步器 / Unity `FixedUpdate` |
| `engine::IGameModule` (`src/engine/game_module.h`) | 定义通用引擎底座与具体游戏业务模块（如 `War3GameModule`、塔防/MOBA/RPG 模块或无头回归测试宿主）的标准生命周期契约（`OnRegisterSubsystems`、`OnInit`、`OnFixedTick`、`OnFrameUpdate`、`OnRender`、`OnShutdown`） | Godot `MainLoop` / Unity `World` 业务插件 |
| `engine::ISimulationModule` & `engine::NullSimulationModule` (`src/engine/simulation_module.h`) | **[Phase 13–15 核心]** 抽象 `CSimulation` 通用 ECS 世界容器、`CPlayer` 通用玩家状态、`CCombatSystem` 通用战斗计算器及 `MeleeAIController` 战术 AI 与具体游戏玩法规则的完整边界契约：涵盖技能目录挂载/卸载、单位原型技能装配、实体入世/阵亡/形态升级蜕变/退世钩子、科技树等效层级解析、科技升级实时属性加成引擎、经济维护费税率、胜负与残局判定、**攻击/护甲克制倍率与护甲减伤曲线（`GetAttackDefenseMultiplier` / `ComputeArmorDamageReduction` / `ShouldResistMagicDamage` / `EvaluateEtherealDamageFactor`）**，以及**战术 AI 托管与护甲克制目标评分（`CreatePlayerAI` / `TickPlayerAIs` / `EvaluateTacticalTargetScore`）**。挂载 `NullSimulationModule` 时 `CSimulation` 即刻进入 **Pure Engine Mode（纯净通用引擎模式）** | Godot `GDExtension` 玩法扩展 / Unity `Authoring & Rule System` |
| `engine::DynamicModuleLoader` (`src/engine/dynamic_module_loader.h/.cpp`) | **[Phase 13 新增]** 统一管理内置模块工厂（`RegisterBuiltInModule`）与操作系统原生共享库（`.so` / `.dll`）运行时动态加载（`LoadSharedLibrary`、`UnloadSharedLibrary`、`HotReloadSharedLibrary`），基于 POSIX `dlopen`/`dlsym`/`dlclose`（及 Windows `LoadLibraryA`/`GetProcAddress`）绑定 C ABI 导出入口，并通过 `shared_ptr` 自定义析构器安全托管跨 DLL 边界实例与代数（Generation）追踪 | Godot `GDExtensionLoader` / Unreal `FModuleManager` |
| `engine::EngineKernel` (`src/engine/engine_kernel.h/.cpp`) | 统一收拢七大核心引擎服务器（`JobSystem`、`ResourceServer`、`AudioServer`、`RenderServer + RenderGraph`、`InputActionMap`、`ScriptHostRouter`、`EngineProfiler`），支持**独立自治模式（`BootstrapStandardSubsystems`）**与**非侵入宿主绑定模式（`BindExternalServices`）**双轨运行 | Godot `OS + Engine` / Unity `PlayerLoop` |

## 2. Phase 13：`CSimulation` 单位/技能体系拆分为独立的 `War3GameModule` 动态模块

* **`CSimulation` 纯净化改造（`src/ecs/csimulation.h/.cpp`）**：移除全部 14 个具体魔兽技能 `#include`，将单位技能装配、物品/可破坏物配置、实体入世/退世结算统一委托给 `simModule_`。

* **模块作用域技能注册表（`src/ecs/ability_factory.h/.cpp`）**：`src/ecs/ability_factory.cpp` 从 **820+ 行** 精简至 **226 行**（缩减 **72%**），支持按 `moduleId` 注册、查询与批量卸载（`RegisterModuleCreator`、`RegisterModuleAlias`、`RegisterModuleSpellDescriptor`、`UnregisterModuleCreators`）。

* **原生共享库插件导出（`src/game/war3/war3_module_plugin_export.cpp` -> `libwar3_module_plugin.so`）**：导出标准 C ABI 入口符号，支持通过 `DynamicModuleLoader` 在运行时 `dlopen` 动态挂载或 `HotReloadSharedLibrary` 在线热重载。

## 3. Phase 14：将 `CPlayer` 科技树/升级体系与胜负判定规则抽离至 `War3GameModule` 插件

* **四族科技树等效推导与建筑升级蜕变同步（`War3GameModule::GetEffectiveTechLevel` & `OnUnitMorphed`）**：内置人族（`htow <- hkee <- hcas`）、兽族（`ogre <- ostr <- ofor`）、不死族（`unpl <- unp1 <- unp2`）、暗夜精灵（`etol <- etoa <- etoe`）及防御塔科技等效树，支持 `RegisterTechEquivalence` 动态扩展；建筑完成升级蜕变（`CAbilityUpgrade`）时自动转移科技计数与人口上限差值。

* **数据驱动的科技升级属性实时生效引擎（`CUpgradeEffectSpec` & `War3GameModule::ApplySingleUpgradeToUnit`）**：完整解析 `UpgradeData.slk` 的 `effect1..4`、`base1..4`、`mod1..4`、`code1..4` 与 `class`，支持护甲（`rarm`）、攻击力（`ratx`/`ratd`/`ratc`）、生命值（`rhpx`/`rhpo`）、法力值（`rmnx`/`rmno`）、移速（`rmvx`）、攻速（`rats`）与技能等级（`rlev`）对场上存活单位与新训练单位的增量幂等生效。

* **对战/同盟胜负判定与残局暴露规则引擎（`War3GameModule::EvaluateVictoryDefeat` & `IsPlayerCrippled`）**：封装无主基地且无农民/资金重建时的残局暴露（`Crippled`）、建筑全灭判负（`DEFEAT`）以及双向同盟共享胜利（`VICTORY`）规则与 `SimEventBus` 信号广播。

## 4. Phase 15：将 `CCombatSystem` 护甲/攻击克制表与 `MeleeAIController` 战术 AI 决策解耦至 `War3GameModule` 插件

### 4.1 `CCombatSystem` 护甲/攻击克制矩阵与减伤规则插件化（`src/ecs/combat/combat_system.h/.cpp`）

* **问题诊断**：重构前，`src/ecs/combat/combat_system.cpp` 内部静态硬编码了魔兽3冰封王座的 7×8 攻击对护甲克制表 `kTable`（普通/穿刺/攻城/魔法/混乱/英雄/法术 vs 无甲/轻甲/中甲/重甲/城甲/英雄/神圣/未武装）、`0.06` 护甲非线性减伤公式以及虚无（Ethereal `1.66x` 魔法加成与物理免疫）/魔免（Magic Immunity）判定规则，导致通用战斗组件无法在不修改源码的情况下切换为纯数值 1:1 伤害模型或其他游戏类型的克制表。

* **解耦方案**：
  1. **标准克制表与伤害规则收归 `War3GameModule`**：
     * `War3GameModule::GetCanonicalWar3AttackDefenseTable()` 封装完整的魔兽3 TFT 7×8 克制矩阵。
     * `War3GameModule` 实现 `ISimulationModule` 的四大战斗策略钩子：`GetAttackDefenseMultiplier`、`ComputeArmorDamageReduction`、`ShouldResistMagicDamage` 与 `EvaluateEtherealDamageFactor`。
     * 支持运行时通过 `War3GameModule::SetAttackDefenseMultiplier(atk, def, mult)`、`SetCombatTable(table)` 与 `SetDefenseArmorFactor(k)` 热修改克制倍率（如自定义地图平衡性补丁），并实时同步至绑定的 `CCombatSystem`。
  2. **Pure Engine Mode 1:1 纯净伤害模式**：
     * 当 `CSimulation` 切换至 `NullSimulationModule`（`SetPureEngineMode(true)`）时，`CCombatSystem` 自动通过 `ISimulationModule` 进入无克制偏置模式：所有攻击与护甲组合倍率恒为 `1.0x`，护甲非线性减伤归零（`0.0f`），彻底剥离魔兽3专属规则。

### 4.2 `MeleeAIController` 战术 AI 生命周期托管与护甲克制集火评分（`src/ecs/melee_ai.h/.cpp`）

* **问题诊断**：
  1. 重构前，`MeleeAIController` 实例仅保存在 JASS 脚本层的 `JassNativesData::aiControllers` 哈希表中，引擎内核与 `War3GameModule` 无法统一管理、步进或热插拔 AI 控制器。
  2. `TacticalThreatMap::ScoreTargetUtility` 在评估集火目标优先级时，仅考虑了残血斩杀分（`executionBonus`）、单位角色分（`roleBonus`）与距离惩罚（`distPenalty`），**忽略了攻击类型对目标护甲类型的克制倍率**（例如穿刺火枪手面对同等距离与血量的轻甲狮鹫骑士 `2.0x` 与城甲单位 `0.35x` 时评分相同）。

* **解耦方案**：
  1. **`War3GameModule` 统一托管 AI 生命周期与四族建造规划推导**：
     * `War3GameModule` 内置 `aiControllers_` 注册表，实现 `CreatePlayerAI`、`AttachPlayerAI`、`GetPlayerAI`、`RemovePlayerAI`、`SetPlayerAIEnabled`、`CommandPlayerAI` 与 `TickPlayerAIs`，并在 `War3GameModule::OnFixedTick` 中自动驱动所有启用状态的 AI 控制器；同时与 JASS `StartMeleeAI`、`CommandAI`、`PauseCompAI` 双向打通。
     * 将四族基地/农民/农场/兵营/祭坛/英雄/防御塔的自动角色发现逻辑提炼为 `War3GameModule::DiscoverRaceBuildPlan`，供 `MeleeAIController::DiscoverPlan()` 委托调用。
  2. **护甲克制感知的效用 AI 集火评分（`War3GameModule::EvaluateTacticalTargetScore`）**：
     * `TacticalThreatMap::ScoreTargetUtility` 在计算基础效用分后，回调 `simModule_->EvaluateTacticalTargetScore(*boundSim_, *attacker, *candidateTarget, baseScore)`。
     * `War3GameModule` 实时查询攻击者主武器 `CAttackType` 对候选目标 `CDefenseType` 的克制倍率 `counterMult`，叠加 `(counterMult - 1.0f) * 90.0f` 的克制权重（例如穿刺打轻甲 `2.0x` 获得 `+90 分` 强集火激励，魔法打重甲 `2.0x` 获得 `+90 分`，而穿刺打城甲 `0.35x` 受到 `-58.5 分` 抑制），使战术微操具备真正的兵种相克智能。

## 5. 云端游戏资源验证与单元测试覆盖

结合云端 Google Drive [`WarCraft3`](https://drive.google.com/drive/folders/1sEyljyjTcnwtxQNPEhYC2dVMh4Idit-L) / [`Maps/FrozenThrone`](https://drive.google.com/drive/folders/1sb1fktbnjqbIEjNrKky4l_iWyMfb_wU3) 下的 14 张零售级对战与 [`Scenario`](https://drive.google.com/drive/folders/18rVJURpX4ZHWLumjNUp0ZeXEN-TJOaj-) 地图（含 [`(2)EchoIsles.w3x`](https://drive.google.com/file/d/1d5qijwX3yNOvm9Xej64YpfSTivPlNGff/view?usp=drivesdk)、[`(4)TurtleRock.w3x`](https://drive.google.com/file/d/106f_pFaSePSYhTJTZezXX4NYGotoG1pY/view?usp=drivesdk)、[`(4)TwistedMeadows.w3x`](https://drive.google.com/file/d/1v51wDc9ilk5BtPbEsPUX0mZVWnA3olix/view?usp=drivesdk)、[`(8)GardenOfWar.w3x`](https://drive.google.com/file/d/1LOvXHMDAyjLaLdZlAg4ubMIAcqNi6qrP/view?usp=drivesdk)、[`(6)BlizzardTD.w3x`](https://drive.google.com/file/d/1NK4On-ccrEEzFzRPsTv625yZY5QXl-h8/view?usp=drivesdk)、[`(8)AzerothGrandPrix.w3x`](https://drive.google.com/file/d/1H-h7ZYHa2qdwykkdNkpSaRDdKuNojLAM/view?usp=drivesdk) 等），全量运行 **829 项** 单元与集成测试：

* **Phase 12 专项测试（`Stage EE1`–`Stage GG1`）**：4 项全通过。

* **Phase 13 专项测试（`Stage HH1`–`Stage II2`）**：4 项全通过。

* **Phase 14 专项测试（`Stage JJ1`–`Stage KK2`）**：4 项全通过。

* **Phase 15 专项测试**：
  * `Stage LL1`: `War3GameModule` 7×8 攻击/护甲克制倍率（穿刺 vs 轻甲 `2.0x`、穿刺 vs 城甲 `0.35x`、魔法 vs 重甲 `2.0x` + 10 点护甲 `37.5%` 减伤）与 `PureEngineMode` 1:1 纯净伤害对照验证。
  * `Stage LL2`: `War3GameModule` 运行时自定义克制倍率热覆盖（`SetAttackDefenseMultiplier`）及 `./libwar3_module_plugin.so`（Version 15）跨 `.so` 边界战斗公式验证。
  * `Stage MM1`: `War3GameModule` 托管 `MeleeAIController` 生命周期、四族建造规划自动发现（`htow`/`hpea`/`hhou`/`hbar`/`hrif`）与兵种护甲克制集火评分（穿刺火枪手优先集火轻甲目标而非城甲目标）验证。
  * `Stage MM2`: 云端零售地图 `(2)EchoIsles.w3x` 端到端加载、`EngineKernel` 固定步进驱动 `War3GameModule::OnFixedTick` 自动步进战术 AI 与战斗系统验证。

* **总计 829 / 829 项测试全部通过（100% 通过率，零回归）**。
