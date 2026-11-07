# 现代化游戏引擎底座抽象与精简重构设计文档 (Phase 12)

本项目以 **WarCraft III (Warsmash C++)** 为实验载体，参考 **Godot 4**（`OS / MainLoop`、Server 单例架构、`SceneTree`、`ResourceServer`）与 **Unity**（`PlayerLoop`、`SubsystemManager`、`DOTS Sparse-Set ECS`、数据驱动 `ScriptableObject`）的设计哲学，在已完成 Phase 1–11 现代化改造的基础上，进一步完成 **通用游戏引擎底座（`engine::*`）抽象解耦** 与 **全库去冗余/过时代码精简（De-Redundancy & Legacy Pruning）**。

## 1. 通用化引擎底座架构抽象（`src/engine/`）

为了将本项目从“单一硬编码的魔兽争霸3模拟器”演进为“可复用、可插拔的通用 RTS/RPG 现代游戏引擎”，Phase 12 抽离了零业务耦合的 `src/engine/` 引擎内核层，将底层服务与上层游戏规则（`IGameModule`）彻底解耦：

| 引擎底座模块 | 核心职责与设计模式 | 对标现代引擎参考 |
| :--- | :--- | :--- |
| `engine::ISubsystem` & `engine::SubsystemRegistry` (`src/engine/engine_subsystem.h`) | 定义 9 阶段确定性生命周期总线（`Platform(0) -> Jobs(50) -> Resource(100) -> Audio(200) -> Scripting(300) -> Simulation(400) -> Presentation(500) -> Render(600) -> UI(700) -> Telemetry(800)`），提供 `O(1)` 类型索引服务定位器（`Register<T>` / `TryGet<T>`）、按阶段升序初始化与逆序安全销毁 | Unity `SubsystemManager` / Godot `Server` 注册表 |
| `engine::FixedTimestepStepper` (`src/engine/fixed_timestep_stepper.h`) | 将固定频率确定性模拟步进（如 20Hz RTS 帧同步或 60Hz 物理步进）与高刷显示器渲染帧率（60/144/240Hz）彻底解耦；内置死亡螺旋钳制（`maxSubStepsPerFrame`）并自动计算 `[0, 1]` 帧间插值因子 `interpolationAlpha` | Godot `MainLoop` 物理插值分步器 / Unity `FixedUpdate` |
| `engine::IGameModule` (`src/engine/game_module.h`) | 定义通用引擎底座与具体游戏业务模块（如 `War3GameModule`、塔防/MOBA/RPG 模块或无头回归测试宿主）的标准生命周期契约（`OnRegisterSubsystems`、`OnInit`、`OnFixedTick`、`OnFrameUpdate`、`OnRender`、`OnShutdown`） | Godot `MainLoop` / Unity `World` 业务插件 |
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
   - `engine/engine_kernel.*`、`engine/engine_subsystem.h`、`engine/fixed_timestep_stepper.h`、`engine/game_module.h`。
   - `ecs/ecs_registry.h`（`SparseComponentPool<T>` 稀疏集连续内存池）、`ecs/sim_systems.h`（声明式 `SimulationSystemPipeline`）、`ecs/spatial_grid.h`（`O(N)` 空间哈希网格）、`ecs/pathing/pathfinding_server.*`（零分配 A* 工作区与流场 `FlowFieldGrid` + RVO 群体避障）。
4. **Layer 3 — 游戏业务模块层（WarCraft III Game Module）**
   - `core/game_controller.*`、`core/world_scene_session.*`、`ecs/csimulation.*`、`ecs/unit.*`、`ecs/abilities/*`、`data/*`、`ui/FdfHudHost.*`。

## 2. 去冗余设计与过时代码清理清单（De-Redundancy & Legacy Code Removal）

在本次重构中，我们系统性排查并清除了早期迭代遗留的原型死代码、四重复制的数据合并逻辑以及过度膨胀的对象字段：

### 2.1 移除 28 个废弃/过时源码与头文件（净删 ~1,400+ 行死代码）

| 清理类别 | 移除的过时文件 | 移除原因与现代替代方案 |
| :--- | :--- | :--- |
| **早期渲染原型 (Phase 0)** | `src/render/terrain.h`, `src/render/terrain.cpp` (`TerrainRenderer` / `simpleTerrain_`) | 早期纯色高度图原型，全库无 `Build()` 调用，已完全被 `TexturedTerrain` + `TerrainChunkGrid` 取代 |
| **废弃软件网格与阴影桩** | `src/render/software_mesh.h/.cpp`, `src/render/shadow_manager.h/.cpp` | 全库 0 引用、未接线的早期实验桩代码，地形高度查询已统一由 `TexturedTerrain` 与 `W3EMap` 承载 |
| **前 FDF 时代旧 UI 组件** | `src/ui/AudioOptionsFrame.h`, `src/ui/SliderFrame.h/.cpp`, `src/ui/HeroPanelFrame.h/.cpp`, `src/ui/InfoPanelFrame.h/.cpp`, `src/ui/PortraitFrame.h/.cpp`, `src/ui/TooltipFrame.h/.cpp`, `src/ui/BuffBar.h/.cpp`, `src/ui/CommandCard.h/.cpp`, `src/ui/ResourceBarFrame.h/.cpp`, `src/ui/ChatConsoleFrame.h/.cpp`, `src/ui/MultiSelectionPanel.h/.cpp` | 在引入零售级 FDF 驱动栈（`FdfHudHost` + `ConsoleHUD` + `ExtendedFrames.h`）后，`MainUI` 中的 `commandCard_`、`resourceBar_`、`chatConsole_` 恒为 `nullptr`，上述 21 个旧 UI 文件已彻底废弃 |
| **废弃 ECS 技能原型头** | `src/ecs/abilities.h` (`struct CAbilityDef`) | 早期占位结构体，已被 `ecs/ability.h` + `data/abilitydata.h` (`CAbilityDefFull`) + `ABSpellDescriptor` 取代 |

### 2.2 合并 `src/data/` 中四重复制的 SLK/TXT/物编合并管线（净减 ~450 行重复代码）

- **问题诊断**：`GAP_ANALYSIS.md` 第 28 项指出 `slk_merge 双实现`。经代码审计发现，`src/data/slk_merge.h/.cpp`、`src/data/itemdata.cpp`、`src/data/unitdata.cpp` 与 `src/data/object_mod_meta.h` 各自拷贝了一份 `SlkAccessor`、`TrimCopy`、`FindFirstColumn`、`GetMergedString/Int`、`FormatObjectModificationValue`、`ResolveObjectModField`、`CopyObjectDataSection` 与物编应用循环。
- **精简方案**：
  1. 在 `src/data/slk_merge.h` 与 `src/data/slk_merge.cpp` 中统一提供双重载 `GetMergedString/Int/Float`（兼顾按行列索引与按 `SlkAccessor` 大小写不敏感查找）、单表/多表双重载 `ResolveObjectModField`、`ParseWar3IdList` 以及通用单表物编合并引擎 `ApplySingleTableObjectMods(...)`。
  2. `ApplyMapDestructableObjectMods`（`w3b`）、`ApplyMapDoodadObjectMods`（`w3d`）、`ApplyMapItemObjectMods`（`w3t`）全部收敛为对 `ApplySingleTableObjectMods` 的 5 行声明式调用。
  3. `src/data/object_mod_meta.h` 改为自持有 `SlkTable` 并直接委托 `slk_merge.h`，修复了局部作用域销毁 `metaSlk` 时的悬垂指针隐患。

### 2.3 `CUnit` 冷数据下沉与内存布局瘦身（单实例体积缩减 17.2%）

- **问题诊断**：每个 `CUnit` 实例无条件内联了 3 个 `std::unordered_set<int>`（`sharedVision_`、`addedUnitTypes_`、`removedUnitTypes_`，共 168 字节桶头开销）、1 个 `std::unordered_map<int, AttackModifierSlot>`（56 字节）、废弃的 `std::vector<CAbilityDef> abilityDefs_`（24 字节）以及只写不读的 `int speed_`。然而 99% 的常规单位从不调用 `UnitShareVision` 或 `UnitAddType`。
- **精简方案**：
  1. 新增 `UnitScriptOverridesComponent`（`src/ecs/unit_components.h`），将 `sharedVisionPlayers`、`addedUnitTypes`、`removedUnitTypes` 下沉至 `SimEcsRegistry` 的 `SparseComponentPool<UnitScriptOverridesComponent>` 中按需懒分配，常规单位实现 **0 字节额外开销**。
  2. 将 `attackModifiers_` 由 `std::unordered_map` 替换为紧凑 `std::vector<AttackModifierSlot>`，移除死字段 `abilityDefs_` 与 `speed_`。
  3. 实测 `sizeof(CUnit)` 从 **1,352 字节** 降至 **1,120 字节**（每个单位节省 **232 字节**，缩减 **17.2%**）。

### 2.4 `GameController` 冗余引用别名清理与动画桥接收敛

- 移除了 `GameController` 中为过渡期兼容保留的 16 个引用成员（`simUnitVisualSeeds`、`simUnitPendingAnimations` 等，节省 128 字节对象体积），统一通过 `UnitPresentationSystem` 访问。
- 将 `world_presentation_bridge.cpp` 中 9 段重复的 JASS 动画控制 lambda 收敛为 `submitOrApplyAnim`、`queueOrApplyAnim` 与 `UnitPresentationSystem::ModifyUnitAnimationPropertyTags` 调用。
- 将 `GameController::StepSimulation(float dt)` 接入 `engineKernel_.Stepper().AdvanceFrame(...)`，消除了手写 `simulationAccumulator_` 步进逻辑。

## 3. 云端游戏资源验证与单元测试覆盖

结合云端 Google Drive `WarCraft3/Maps/FrozenThrone` 下的 14 张零售级对战与 Scenario 地图（含 `(2)EchoIsles.w3x`、`（4）TurtleRock.w3x`、`（4）TwistedMeadows.w3x`、`（8）GardenOfWar.w3x`、`（8）AzerothGrandPrix.w3x`、`（10）ExtremeCandyWar2004.w3x` 等），全量运行 **817 项** 单元与集成测试（含新增的 Phase 12 `Stage EE1`、`Stage EE2`、`Stage FF1`、`Stage GG1` 四大引擎底座与精简重构专项测试），**817 / 817 全部通过（零回归）**。
