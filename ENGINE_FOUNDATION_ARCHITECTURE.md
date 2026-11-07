# 现代化游戏引擎底座抽象与精简重构设计文档 (Phase 12 & Phase 13)

本项目以 **WarCraft III (Warsmash C++)** 为实验载体，参考 **Godot 4**（`OS / MainLoop`、Server 单例架构、`GDExtension` 动态插件系统、`SceneTree`、`ResourceServer`）与 **Unity**（`PlayerLoop`、`SubsystemManager`、`DOTS Sparse-Set ECS`、可热插拔 `GamePlay Assembly`、数据驱动 `ScriptableObject`）的设计哲学，在已完成 Phase 1–11 现代化改造的基础上，进一步完成：
- **Phase 12**：**通用游戏引擎底座（`engine::*`）抽象解耦** 与 **全库去冗余/过时代码精简（De-Redundancy & Legacy Pruning）**。
- **Phase 13**：**将 `CSimulation` 内的魔兽争霸3单位/技能体系彻底拆分为独立的 `war3::War3GameModule` 动态可插拔模块（含 `engine::ISimulationModule` 模拟策略接口、模块作用域技能目录管理与 OS 原生 `.so`/`.dll` `dlopen` 动态热重载器 `engine::DynamicModuleLoader`）**。

## 1. 通用化引擎底座架构抽象（`src/engine/`）

为了将本项目从“单一硬编码的魔兽争霸3模拟器”演进为“可复用、可插拔的通用 RTS/RPG 现代游戏引擎”，Phase 12–13 抽离了零业务耦合的 `src/engine/` 引擎内核层，将底层服务与上层游戏规则（`IGameModule` + `ISimulationModule`）彻底解耦：

| 引擎底座模块 | 核心职责与设计模式 | 对标现代引擎参考 |
| :--- | :--- | :--- |
| `engine::ISubsystem` & `engine::SubsystemRegistry` (`src/engine/engine_subsystem.h`) | 定义 9 阶段确定性生命周期总线（`Platform(0) -> Jobs(50) -> Resource(100) -> Audio(200) -> Scripting(300) -> Simulation(400) -> Presentation(500) -> Render(600) -> UI(700) -> Telemetry(800)`），提供 `O(1)` 类型索引服务定位器（`Register<T>` / `TryGet<T>`）、按阶段升序初始化与逆序安全销毁 | Unity `SubsystemManager` / Godot `Server` 注册表 |
| `engine::FixedTimestepStepper` (`src/engine/fixed_timestep_stepper.h`) | 将固定频率确定性模拟步进（如 20Hz RTS 帧同步或 60Hz 物理步进）与高刷显示器渲染帧率（60/144/240Hz）彻底解耦；内置死亡螺旋钳制（`maxSubStepsPerFrame`）并自动计算 `[0, 1]` 帧间插值因子 `interpolationAlpha` | Godot `MainLoop` 物理插值分步器 / Unity `FixedUpdate` |
| `engine::IGameModule` (`src/engine/game_module.h`) | 定义通用引擎底座与具体游戏业务模块（如 `War3GameModule`、塔防/MOBA/RPG 模块或无头回归测试宿主）的标准生命周期契约（`OnRegisterSubsystems`、`OnInit`、`OnFixedTick`、`OnFrameUpdate`、`OnRender`、`OnShutdown`） | Godot `MainLoop` / Unity `World` 业务插件 |
| `engine::ISimulationModule` & `engine::NullSimulationModule` (`src/engine/simulation_module.h`) | **[Phase 13 新增]** 抽象 `CSimulation` 通用 ECS 世界容器与具体游戏玩法规则的边界契约：负责技能目录挂载/卸载（`OnRegisterAbilityCatalog` / `OnUnregisterAbilityCatalog`）、单位原型技能自动装配（`OnAssembleUnitAbilities`）、物品与可破坏物初始化（`OnConfigureCreatedItem` / `OnConfigureCreatedDestructable`）以及实体入世/退世的科技树与人口结算钩子（`OnUnitCommittedToWorld` / `OnUnitRemovedFromWorld`）。挂载 `NullSimulationModule` 时 `CSimulation` 即刻进入 **Pure Engine Mode（纯净通用引擎模式）** | Godot `GDExtension` 玩法扩展 / Unity `Authoring & Baker System` |
| `engine::DynamicModuleLoader` (`src/engine/dynamic_module_loader.h/.cpp`) | **[Phase 13 新增]** 统一管理内置模块工厂（`RegisterBuiltInModule`）与操作系统原生共享库（`.so` / `.dll`）运行时动态加载（`LoadSharedLibrary`、`UnloadSharedLibrary`、`HotReloadSharedLibrary`），基于 POSIX `dlopen`/`dlsym`/`dlclose`（及 Windows `LoadLibraryA`/`GetProcAddress`）绑定 C ABI 导出入口，并通过 `shared_ptr` 自定义析构器安全托管跨 DLL 边界实例与代数（Generation）追踪 | Godot `GDExtensionLoader` / Unreal `FModuleManager` |
| `engine::EngineKernel` (`src/engine/engine_kernel.h/.cpp`) | 统一收拢七大核心引擎服务器（`JobSystem`、`ResourceServer`、`AudioServer`、`RenderServer + RenderGraph`、`InputActionMap`、`ScriptHostRouter`、`EngineProfiler`），支持**独立自治模式（`BootstrapStandardSubsystems`）**与**非侵入宿主绑定模式（`BindExternalServices`）**双轨运行 | Godot `OS + Engine` / Unity `PlayerLoop` |

### 引擎分层全景图

1. **Layer 0 — 平台与并发原语层（Platform & Concurrency）**
   - `platform/*`、`core/job_system.*`（多线程工作池与 `ParallelFor`）、`core/signal.h`（多播信号槽）、`core/engine_profiler.*`（分层采样与 Chrome Trace 导出）。
2. **Layer 1 — 引擎服务器层（Engine Servers — Godot Server Pattern）**
   - **虚拟文件系统与资源服务器 (`resource/resource_server.*`)**：统一 `mpq://`、`casc://`、`map://` URI 规范化、`RID` 句柄缓存、后台异步预加载与引用计数 GC。
   - **渲染服务器与声明式渲染图 (`render/render_server.*`, `render/render_graph.*`)**：7 通道声明式 RenderGraph、视锥 3 态分块裁剪（`StaticSpatialChunkGrid` / `TerrainChunkGrid`）、共享网格注册表（`MdxSharedMeshRegistry`）、硬件实例化批次合批与 `BonePaletteAtlas`（RGBA32F 骨骼矩阵纹理图集）。
   - **音频混音服务器 (`audio/audio_server.*`)**：分层混音总线（Master / Music / SFX / UnitVoice / UI / Ambient）、语音侧链闪避（Sidechain Ducking）、3D 距离衰减与优先级偷声（Voice Stealing）。
   - **多后端脚本宿主路由 (`scripting/script_engine_host.*`)**：统一 `IScriptEngineHost` 契约，支持 `JassVM` 字节码协程后端与 `RealScriptEngineHost` 运行时热切换（Hot-Swap）。
3. **Layer 2 — 通用引擎内核与世界框架层（Engine Kernel & ECS World）**
   - `engine/engine_kernel.*`、`engine/engine_subsystem.h`、`engine/fixed_timestep_stepper.h`、`engine/game_module.h`、`engine/simulation_module.h`、`engine/dynamic_module_loader.*`。
   - `ecs/csimulation.*`（零 War3 规则硬编码的确定性 ECS 实体世界与句柄管理器）、`ecs/ability_factory.*`（支持按 `moduleId` 隔离挂载与批量卸载的通用技能工厂注册表）。
   - `ecs/ecs_registry.h`（`SparseComponentPool<T>` 稀疏集连续内存池）、`ecs/sim_systems.h`（声明式 `SimulationSystemPipeline`）、`ecs/spatial_grid.h`（`O(N)` 空间哈希网格）、`ecs/pathing/pathfinding_server.*`（零分配 A* 工作区与流场 `FlowFieldGrid` + RVO 群体避障）。
4. **Layer 3 — 独立可热插拔游戏业务模块层（`src/game/war3/` — `War3GameModule` & `libwar3_module_plugin.so`）**
   - `game/war3/war3_game_module.h/.cpp`、`game/war3/war3_module_plugin_export.cpp`：封装全部 ~190 个魔兽争霸3标准技能/法术描述符注册与四族工人/英雄/城镇大厅/建筑/商店原型装配规则，既可静态链接也可编译为独立 `libwar3_module_plugin.so` 动态共享库按需挂载或运行时热重载。

## 2. Phase 13：`CSimulation` 单位/技能体系拆分为独立的 `War3GameModule` 动态模块

### 2.1 重构前痛点诊断
在 Phase 12 之前，`CSimulation` (`src/ecs/csimulation.cpp`) 与 `AbilityFactory` (`src/ecs/ability_factory.cpp`) 存在严重的引擎层与魔兽3业务层耦合：
1. **`CSimulation` 直接包含 14 个魔兽3具体技能头文件**（`ability_attack.h`、`ability_move.h`、`ability_harvest.h`、`ability_build.h`、`ability_queue.h`、`ability_upgrade.h`、`ability_revive.h`、`ability_hero.h`、`ability_inventory.h`、`ability_rally.h`、`ability_return_resources.h`、`ability_goldmine.h`、`ability_blight.h`、`ability_shop.h`）。
2. **`CSimulation::CreateUnit` 硬编码了近 200 行魔兽3专属种族与建筑规则**：例如硬编码检查 `"uaco"`（不死族侍僧不采木）、`"ewsp"`（暗夜精灵小精灵采木量）、四族建造技能 FourCC（`AHbu`/`AObu`/`AEbu`/`AUbu`）、英雄复活祭坛与商店柜台逻辑，以及人口占用（`foodUsed`/`foodMade`）和科技树计数更新。
3. **`AbilityFactory` 静态硬编码了 ~190 个魔兽3技能与法术（`AHbz`、`AOww`、`AEbl`、`AUdc` 等）**：无法按模块卸载或隔离，导致引擎底座无法在不携带魔兽3技能表的情况下运行其他类型游戏（如科幻 RTS、塔防、生存肉鸽）。

### 2.2 核心解耦架构与实现细节

#### （1）`CSimulation` 纯净化改造（`src/ecs/csimulation.h/.cpp`）
- **移除全部 14 个具体技能 `#include`**：`src/ecs/csimulation.cpp` 不再直接引用任何 `ecs/abilities/ability_*.h` 具体技能实现类，编译依赖大幅减轻。
- **委托 `engine::ISimulationModule` 策略接口**：
  - `CreateUnit` 仅负责分配 `handleId`、在 `SimEcsRegistry` 中初始化通用 ECS 组件（Transform、Stats、Movement、Combat、Pathing）与空间网格坐标，随后调用 `simModule_->OnAssembleUnitAbilities(*this, *unitPtr, *unitType)` 完成业务技能挂载。
  - 实体正式入世（`CreateUnit` 提交及 `ProcessPendingCommits`）与退世（`RemoveUnit`）统一回调 `simModule_->OnUnitCommittedToWorld(*this, *u)` 与 `simModule_->OnUnitRemovedFromWorld(*this, *u)`。
  - 新增 `SetSimulationModule(std::shared_ptr<engine::ISimulationModule>)`、`SetPureEngineMode(bool)` 与 `IsPureEngineMode()`：当开启 `SetPureEngineMode(true)` 时，`CSimulation` 切换至 `engine::NullSimulationModule`，创建的 `CUnit` 为 **0 魔兽技能、0 魔兽人口规则** 的纯净 ECS 实体。

#### （2）模块作用域技能注册表（`src/ecs/ability_factory.h/.cpp`）
- `src/ecs/ability_factory.cpp` 从 **820+ 行** 精简至 **226 行**（缩减 **72%**），不再包含任何具体魔兽技能头文件。
- 新增模块级管理 API：
  - `AbilityFactory::RegisterModuleCreator(moduleId, code, creator)`
  - `AbilityFactory::RegisterModuleAlias(moduleId, alias, targetCode)`
  - `AbilityFactory::RegisterModuleSpellDescriptor(moduleId, desc)`
  - `AbilityFactory::UnregisterModuleCreators(moduleId)`：支持在模块卸载或热重载时，精确清理由指定 `moduleId`（如 `"War3GameModule"` 或自定义扩展模块 `"SciFiTerranModule"`）注册的所有技能与 `AbilityBuilderRegistry` 描述符，互不干扰。
  - `AbilityFactory::GetModuleRegisteredCount(moduleId)` 与 `AbilityFactory::GetCreatorModuleId(code)`：提供模块归属自省能力。

#### （3）独立的 `war3::War3GameModule` 动态模块（`src/game/war3/`）
- **`war3_game_module.h/.cpp`**：同时实现 `engine::IGameModule`（引擎主循环生命周期）与 `engine::ISimulationModule`（模拟层实体装配与规则钩子）：
  - `RegisterWar3AbilityCatalog()` / `UnregisterWar3AbilityCatalog()`：集中管理全部 ~190 个魔兽3基础技能、英雄技能、物品技能与数据驱动法术描述符，归属模块 ID 为 `"War3GameModule"`。
  - `OnAssembleUnitAbilities(...)`：完整封装魔兽3攻击（`Aatk`）、移动（`Amov`）、四族采集（`Ahar` + `uaco`/`ewsp` 定制）、四族建造（`AHbu`/`AObu`/`AEbu`/`AUbu`）、训练队列（`Aque`）、集结点（`ARal`）、建筑升级（`Aupg`）、英雄复活祭坛（`Arev`）、金矿/返还资源（`Agld`/`Aret`）、商店（`Ane2`/`Apit`/`Asud`）、英雄面板与物品栏（`AHer`/`AInv`）及 SLK `abilList` 装配。
  - 实时统计 `engine::SimulationModuleStats`（`unitsAssembled`、`abilitiesAttached`、`itemsConfigured`、`destructablesConfigured`、`unitsCommitted`、`unitsRemoved`）。
- **`war3_module_plugin_export.cpp` & `libwar3_module_plugin.so`**：
  - 导出标准 C ABI 符号：`WarsmashGetModulePluginId`、`WarsmashCreateSimulationModule`、`WarsmashDestroySimulationModule`、`WarsmashCreateGameModule`、`WarsmashDestroyGameModule`。
  - 通过 `engine::DynamicModuleLoader::Instance().LoadSharedLibrary("War3DynamicPluginSO", "./libwar3_module_plugin.so")` 即可在运行时通过 OS `dlopen`/`dlsym` 动态加载魔兽3模块，并支持 `HotReloadSharedLibrary` 在线热重载（Generation 自动递增）。

## 3. Phase 12 去冗余设计与过时代码清理回顾（De-Redundancy & Legacy Code Removal）

### 3.1 移除 28 个废弃/过时源码与头文件（净删 ~1,400+ 行死代码）

| 清理类别 | 移除的过时文件 | 移除原因与现代替代方案 |
| :--- | :--- | :--- |
| **早期渲染原型 (Phase 0)** | `src/render/terrain.h`, `src/render/terrain.cpp` (`TerrainRenderer` / `simpleTerrain_`) | 早期纯色高度图原型，全库无 `Build()` 调用，已完全被 `TexturedTerrain` + `TerrainChunkGrid` 取代 |
| **废弃软件网格与阴影桩** | `src/render/software_mesh.h/.cpp`, `src/render/shadow_manager.h/.cpp` | 全库 0 引用、未接线的早期实验桩代码，地形高度查询已统一由 `TexturedTerrain` 与 `W3EMap` 承载 |
| **前 FDF 时代旧 UI 组件** | `src/ui/AudioOptionsFrame.h`, `src/ui/SliderFrame.h/.cpp`, `src/ui/HeroPanelFrame.h/.cpp`, `src/ui/InfoPanelFrame.h/.cpp`, `src/ui/PortraitFrame.h/.cpp`, `src/ui/TooltipFrame.h/.cpp`, `src/ui/BuffBar.h/.cpp`, `src/ui/CommandCard.h/.cpp`, `src/ui/ResourceBarFrame.h/.cpp`, `src/ui/ChatConsoleFrame.h/.cpp`, `src/ui/MultiSelectionPanel.h/.cpp` | 在引入零售级 FDF 驱动栈（`FdfHudHost` + `ConsoleHUD` + `ExtendedFrames.h`）后，`MainUI` 中的 `commandCard_`、`resourceBar_`、`chatConsole_` 恒为 `nullptr`，上述 21 个旧 UI 文件已彻底废弃 |
| **废弃 ECS 技能原型头** | `src/ecs/abilities.h` (`struct CAbilityDef`) | 早期占位结构体，已被 `ecs/ability.h` + `data/abilitydata.h` (`CAbilityDefFull`) + `ABSpellDescriptor` 取代 |

### 3.2 合并 `src/data/` 中四重复制的 SLK/TXT/物编合并管线（净减 ~450 行重复代码）
- 在 `src/data/slk_merge.h` 与 `src/data/slk_merge.cpp` 中统一提供双重载 `GetMergedString/Int/Float`、单表/多表双重载 `ResolveObjectModField`、`ParseWar3IdList` 以及通用单表物编合并引擎 `ApplySingleTableObjectMods(...)`。
- `ApplyMapDestructableObjectMods`（`w3b`）、`ApplyMapDoodadObjectMods`（`w3d`）、`ApplyMapItemObjectMods`（`w3t`）全部收敛为对 `ApplySingleTableObjectMods` 的 5 行声明式调用。

### 3.3 `CUnit` 冷数据下沉与内存布局瘦身（单实例体积缩减 17.2%）
- 新增 `UnitScriptOverridesComponent`（`src/ecs/unit_components.h`），将 `sharedVisionPlayers`、`addedUnitTypes`、`removedUnitTypes` 下沉至 `SimEcsRegistry` 的 `SparseComponentPool<UnitScriptOverridesComponent>` 中按需懒分配，常规单位实现 **0 字节额外开销**。
- 实测 `sizeof(CUnit)` 从 **1,352 字节** 降至 **1,120 字节**（每个单位节省 **232 字节**，缩减 **17.2%**）。

## 4. 云端游戏资源验证与单元测试覆盖

结合云端 Google Drive [`WarCraft3`](https://drive.google.com/drive/folders/1sEyljyjTcnwtxQNPEhYC2dVMh4Idit-L) / [`Maps/FrozenThrone`](https://drive.google.com/drive/folders/1sb1fktbnjqbIEjNrKky4l_iWyMfb_wU3) 下的 14 张零售级对战与 [`Scenario`](https://drive.google.com/drive/folders/18rVJURpX4ZHWLumjNUp0ZeXEN-TJOaj-) 地图（含 [`(2)EchoIsles.w3x`](https://drive.google.com/file/d/1d5qijwX3yNOvm9Xej64YpfSTivPlNGff/view?usp=drivesdk)、[`(4)TurtleRock.w3x`](https://drive.google.com/file/d/106f_pFaSePSYhTJTZezXX4NYGotoG1pY/view?usp=drivesdk)、[`(4)TwistedMeadows.w3x`](https://drive.google.com/file/d/1v51wDc9ilk5BtPbEsPUX0mZVWnA3olix/view?usp=drivesdk)、[`(8)GardenOfWar.w3x`](https://drive.google.com/file/d/1LOvXHMDAyjLaLdZlAg4ubMIAcqNi6qrP/view?usp=drivesdk)、[`(6)BlizzardTD.w3x`](https://drive.google.com/file/d/1NK4On-ccrEEzFzRPsTv625yZY5QXl-h8/view?usp=drivesdk)、[`(8)AzerothGrandPrix.w3x`](https://drive.google.com/file/d/1H-h7ZYHa2qdwykkdNkpSaRDdKuNojLAM/view?usp=drivesdk) 等），全量运行 **821 项** 单元与集成测试：
- **Phase 12 专项测试（`Stage EE1`、`Stage EE2`、`Stage FF1`、`Stage GG1`）**：4 项全通过。
- **Phase 13 专项测试**：
  - `Stage HH1`: `War3GameModule` 单位与技能装配解耦 vs `PureEngineMode` (`NullSimulationModule`) 对照验证。
  - `Stage HH2`: `AbilityFactory` 模块作用域技能目录挂载、自定义模块（`SciFiTerranModule`）动态注入与独立卸载验证。
  - `Stage II1`: `DynamicModuleLoader` 基于 `dlopen`/`dlsym` 加载 `./libwar3_module_plugin.so` 原生共享库、跨 `.so` 边界装配 `CSimulation` 单位及运行时 `HotReloadSharedLibrary` 热重载代数递增验证。
  - `Stage II2`: 动态创建的 `War3GameModule` 在多张云端冰封王座零售地图（Melee + Scenario）上的 `WorldSceneSession` 端到端加载与 `EngineKernel` 固定步进驱动验证。
- **总计 821 / 821 项测试全部通过（100% 通过率，零回归）**。
