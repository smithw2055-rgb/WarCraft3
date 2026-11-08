# 现代化游戏引擎底座抽象与精简重构设计文档 (Phase 12 – Phase 21)

本项目以 **WarCraft III (Warsmash C++)** 为实验载体，参考 **Godot 4**（`OS / MainLoop`、Server 单例架构、`GDExtension` 动态插件系统、`FileAccess / DirAccess` 虚拟归档层、`SceneTree`、`ResourceServer`）与 **Unity**（`PlayerLoop`、`SubsystemManager`、`DOTS Sparse-Set ECS`、可热插拔 `GamePlay Assembly`、数据驱动 `ScriptableObject`）的设计哲学，在已完成 Phase 1–11 现代化改造的基础上，进一步完成：

* **Phase 12**：**通用游戏引擎底座（`engine::*`）抽象解耦** 与 **全库去冗余/过时代码精简（De-Redundancy & Legacy Pruning）**。
* **Phase 13**：**将 `CSimulation` 内的魔兽争霸3单位/技能体系彻底拆分为独立的 `war3::War3GameModule` 动态可插拔模块（含 `engine::ISimulationModule` 模拟策略接口、模块作用域技能目录管理与 OS 原生 `.so`/`.dll` `dlopen` 动态热重载器 `engine::DynamicModuleLoader`）**。
* **Phase 14**：**继续将 `CPlayer` 科技树/升级体系（Techtree Tier Equivalences & Live Upgrade Effect Engine）、维护费税率（Upkeep Tax）与对战/同盟胜负及残局暴露判定规则（Melee & Allied Victory/Defeat & Crippled Rules）进一步抽离至 `war3::War3GameModule` 动态插件中**。
* **Phase 15**：**继续将 `CCombatSystem` 7×8 护甲/攻击类型克制表、0.06 护甲减伤曲线、虚无/魔免规则，以及 `MeleeAIController` 战术 AI 生命周期托管、四族建造规划推导与护甲克制优先集火评分（Armor-Counter Tactical Target Scoring）全面解耦至 `war3::War3GameModule` 动态插件中**。
* **Phase 16**：**完成 `CUnit`、数据层 `CUnitData`、物品属性管线 `item_bonuses`、战役跨图英雄继承 `hero_carry` 与核心头文件体系的全面去耦合与精简：彻底移除 `src/ecs/unit.cpp` 内全部 10 个具体魔兽技能头文件（`unit.cpp` 缩减 40.7%），将 `CUnit` 16 个散落布尔字段压缩为 2 字节 `uint16_t` 位域标志（`UnitStateFlag`，内存占用缩减 87.5%），并将伤害反应、命中被动、击杀赏金骰子、英雄经验分配、空闲自动施法、物品三维加成与战役跨图快照收归 `war3::War3GameModule` (Version 16)**。
* **Phase 17**：**完成 `src/ecs/` 核心层与 `src/ecs/behaviors/*` 全部 6 大行为状态机的零具体技能头文件解耦，并在 `CAbilitySpellBase` 中提炼 8 大声明式法术效果原语与统一攻击/物品属性组件（`CAttackBonusDamageEffect`、`CAbilityItemStatBonus::StatSlot`），将 `War3GameModule` 升级至 Version 17（新增 `RegisterCustomSpellDescriptor`）**。
* **Phase 18**：**完成 `CBuff` 全家族 CRTP 自动克隆（`CBuffCloneable<Derived, Base>`）与状态/周期 AoE 辅助内核合并去重（`src/ecs/buffs.cpp` 缩减 54.9%），对 `CAbility`（-40%）、`CBuff`（-28.6%）与 `CAbilitySpellBase`（-22.2%）实施紧凑位域压缩，并将 `War3GameModule` 升级至 Version 18（新增动态自动施法注册接口 `RegisterAutocastAbility` 与 `GetAbilityAutocastKind`）**。
* **Phase 19**：**完成脚本层（`src/scripting/*`）、UI 层（`src/ui/*`）与核心控制层（`src/core/*`）共 26 处具体魔兽技能头文件的彻底清零与 JASS Native 绑定精简（提炼 `ResolveJassUnit`、`FindHeroAbility`、`FindUnitInventory`、`FindUnitShop`、`FindAllWorldShops`），并将 `War3GameModule` 升级至 Version 19（新增 `OnSnapBuildPosition` 与 `IsBuildLocationObstructed` 建筑网格吸附与占地阻挡契约）**。
* **Phase 20**：**完成 `src/map/map_loader.cpp`、`src/ecs/melee_ai.cpp` 及 6 个技能实现文件内部跨技能头文件 `#include` 的彻底清零；实现声明式光环引擎（`CAbilityAura` 1-bit 位域压缩 + `SetAuraBuff` 周期 Buff 刷新工厂 + `War3GameModule::RegisterCustomAura`，使 `src/ecs/abilities/auras.cpp` 缩减 64.6%）；重构 `COrder` 为 `O(1)` 直接值访问并新增运行时动态命令注册表（`RegisterCustomOrder`），同时对 `ability_builder.cpp` 与 `melee_ai.cpp` 实施深度消重，将 `War3GameModule` 升级至 Version 20**。
* **Phase 21**：**完成归档文件系统封装解耦（将散落在 `src/map/`、`src/core/`、`src/scripting/` 共 10 个外部文件中的 `<StormLib.h>` 直接依赖与重复 `SFileOpenArchive` 样板全部收拢至 `MpqManager::ReadSingleArchiveFile` 与 `MpqManager::ReadJassScriptFile`）、地图物编分层加载器与 SLK/TXT 数据表加载管线合并消重（`object_mods.cpp` 7 个 `LoadWar3Map*Mods` 合并为 `LoadWar3MapObjectModsWithLayeredFallbacks`；`unitdata.cpp` Overlay TXT 归档读取次数减少 80%），并将 `War3GameModule` 升级至 Version 21（新增运行时内存态物编原型注入接口 `RegisterCustomItemArchetype`、`RegisterCustomUpgradeArchetype`、`RegisterCustomAbilityDef`）**。

## 1. 通用化引擎底座架构抽象（`src/engine/`）

为了将本项目从“单一硬编码的魔兽争霸3模拟器”演进为“可复用、可插拔的通用 RTS/RPG 现代游戏引擎”，Phase 12–21 抽离了零业务耦合的 `src/engine/` 引擎内核层，将底层服务与上层游戏规则（`IGameModule` + `ISimulationModule`）彻底解耦：

| 引擎底座模块 | 核心职责与设计模式 | 对标现代引擎参考 |
| ----- | ----- | ----- |
| `engine::ISubsystem` & `engine::SubsystemRegistry` (`src/engine/engine_subsystem.h`) | 定义 9 阶段确定性生命周期总线（`Platform(0) -> Jobs(50) -> Resource(100) -> Audio(200) -> Scripting(300) -> Simulation(400) -> Presentation(500) -> Render(600) -> UI(700) -> Telemetry(800)`），提供 `O(1)` 类型索引服务定位器（`Register<T>` / `TryGet<T>`）、按阶段升序初始化与逆序安全销毁 | Unity `SubsystemManager` / Godot `Server` 注册表 |
| `engine::FixedTimestepStepper` (`src/engine/fixed_timestep_stepper.h`) | 将固定频率确定性模拟步进（如 20Hz RTS 帧同步或 60Hz 物理步进）与高刷显示器渲染帧率（60/144/240Hz）彻底解耦；内置死亡螺旋钳制（`maxSubStepsPerFrame`）并自动计算 `[0, 1]` 帧间插值因子 `interpolationAlpha` | Godot `MainLoop` 物理插值分步器 / Unity `FixedUpdate` |
| `engine::IGameModule` (`src/engine/game_module.h`) | 定义通用引擎底座与具体游戏业务模块（如 `War3GameModule`、塔防/MOBA/RPG 模块或无头回归测试宿主）的标准生命周期契约（`OnRegisterSubsystems`、`OnInit`、`OnFixedTick`、`OnFrameUpdate`、`OnRender`、`OnShutdown`） | Godot `MainLoop` / Unity `World` 业务插件 |
| `engine::ISimulationModule` & `engine::NullSimulationModule` (`src/engine/simulation_module.h`) | **\[Phase 13–21 核心\]** 抽象 `CSimulation` 通用 ECS 世界容器、`CUnit` 通用实体、`CPlayer` 通用玩家状态、`CCombatSystem` 通用战斗计算器及 `MeleeAIController` 战术 AI 与具体游戏玩法规则的完整边界契约：涵盖技能目录挂载/卸载、单位原型技能装配与数据绑定、实体生命周期钩子、科技树等效层级解析、科技升级实时属性加成引擎、经济维护费税率、胜负与残局判定、攻击/护甲克制倍率与护甲减伤曲线、战术 AI 托管与种族建造规划发现（`OnDiscoverAIRacePlan`）、伤害前置/后置修正、命中被动触发、击杀赏金与英雄经验分配、空闲自动施法与动态注册（`GetAbilityAutocastKind` / `RegisterAutocastAbility`）、物品属性路由、战役跨图英雄快照恢复、运行时声明式法术/光环/命令动态注册（`RegisterCustomSpellDescriptor` / `RegisterCustomAura` / `RegisterCustomOrder`）、建筑建造网格吸附与占地碰撞校验（`OnSnapBuildPosition` / `IsBuildLocationObstructed`），以及 **\[Phase 21 新增\] 运行时内存态物编原型注入（`RegisterCustomItemArchetype` / `RegisterCustomUpgradeArchetype` / `RegisterCustomAbilityDef`）**。挂载 `NullSimulationModule` 时 `CSimulation` 即刻进入 **Pure Engine Mode（纯净通用引擎模式）** | Godot `GDExtension` 玩法扩展 / Unity `Authoring & Rule System` |
| `engine::DynamicModuleLoader` (`src/engine/dynamic_module_loader.h/.cpp`) | **\[Phase 13 新增\]** 统一管理内置模块工厂（`RegisterBuiltInModule`）与操作系统原生共享库（`.so` / `.dll`）运行时动态加载（`LoadSharedLibrary`、`UnloadSharedLibrary`、`HotReloadSharedLibrary`），基于 POSIX `dlopen`/`dlsym`/`dlclose`（及 Windows `LoadLibraryA`/`GetProcAddress`）绑定 C ABI 导出入口，并通过 `shared_ptr` 自定义析构器安全托管跨 DLL 边界实例与代数（Generation）追踪 | Godot `GDExtensionLoader` / Unreal `FModuleManager` |
| `engine::EngineKernel` (`src/engine/engine_kernel.h/.cpp`) | 统一收拢七大核心引擎服务器（`JobSystem`、`ResourceServer`、`AudioServer`、`RenderServer + RenderGraph`、`InputActionMap`、`ScriptHostRouter`、`EngineProfiler`），支持**独立自治模式（`BootstrapStandardSubsystems`）**与**非侵入宿主绑定模式（`BindExternalServices`）**双轨运行 | Godot `OS + Engine` / Unity `PlayerLoop` |

## 2. Phase 13–20 核心解耦与精简里程碑回顾

* **Phase 13（`CSimulation` 单位/技能体系插件化）**：移除 `src/ecs/csimulation.cpp` 全部 14 个具体魔兽技能 `#include`，`src/ecs/ability_factory.cpp` 缩减 76%，支持 `libwar3_module_plugin.so` 动态加载与热重载。
* **Phase 14（`CPlayer` 科技树/升级与胜负判定插件化）**：封装四族科技树等效推导、`UpgradeData.slk` 实时属性升级引擎、维护费税率与对战/同盟胜负及残局暴露判定。
* **Phase 15（`CCombatSystem` 克制矩阵与 `MeleeAIController` 战术 AI 插件化）**：抽离 7×8 攻击/护甲克制表、0.06 护甲减伤曲线与战术 AI 护甲克制集火评分。
* **Phase 16（`CUnit` 零技能头文件耦合与 `UnitStateFlag` 位域压缩）**：移除 `src/ecs/unit.cpp` 全部 10 个具体魔兽技能头文件（`unit.cpp` 缩减 40.7%），将 `CUnit` 16 个布尔标志压缩为 2 字节 `uint16_t` 位域（缩减 87.5%）。
* **Phase 17（`src/ecs/` 与 `src/ecs/behaviors/*` 零具体技能头文件解耦 + 8 大声明式法术原语）**：`src/ecs/` 顶层及 `src/ecs/behaviors/*` 对具体技能头文件的 14 处 `#include` 清零；精简 18 个法术文件并支持 `RegisterCustomSpellDescriptor`。
* **Phase 18（`CBuffCloneable` CRTP 消重 + `CAbility`/`CBuff` 位域压缩 + 动态自动施法注册）**：消除全部 48 个 `CBuff` 派生类的手写 `Clone()`（`src/ecs/buffs.cpp` 从 1,257 行降至 567 行，缩减 54.9%），压缩 `CAbility`/`CBuff`/`CAbilitySpellBase` 内存占用 20%–40%，并新增 `RegisterAutocastAbility`。
* **Phase 19（`src/scripting/`、`src/ui/`、`src/core/` 26 处具体技能头文件清零与 JASS Native 绑定精简）**：`src/ui/`（10 -> 0）、`src/core/`（9 -> 0）、`src/scripting/`（7 -> 0）共 26 处具体技能头文件 `#include` 全部清零；提炼 `ResolveJassUnit`、`FindHeroAbility`、`FindUnitInventory`、`FindUnitShop`、`FindAllWorldShops`，并将 `War3GameModule` 升级至 Version 19（新增 `OnSnapBuildPosition` 与 `IsBuildLocationObstructed`）。
* **Phase 20（全库残留跨技能头文件彻底清零 + 声明式光环引擎 + `COrder` `O(1)` 直接值访问与动态命令注册表）**：移除 `map_loader.cpp`、`melee_ai.cpp` 及 6 个技能实现文件内部的跨技能头文件 `#include`；将 `src/ecs/abilities/auras.cpp` 从 192 行精简至 68 行（缩减 64.6%）并新增 `RegisterCustomAura`；消除命令分发热路径上的 `dynamic_cast<COrder*>` 并新增 `RegisterCustomOrder`。

## 3. Phase 21：归档文件系统封装（10 处 `<StormLib.h>` 外部依赖清零）、SLK/TXT/物编分层加载消重与 `War3GameModule` Version 21 内存态物编注入

### 3.1 诊断与精简目标

在完成 Phase 20 后，对资源归档层（`src/resource/`）、地图解析层（`src/map/`）、数据表层（`src/data/`）及脚本环境加载器（`src/scripting/jass_*_env.cpp`）的深度审计发现了三大结构性冗余与跨层耦合问题：

1. **10 个外部文件直接 `#include <StormLib.h>` 并各自拷贝 20 行 `SFileOpenArchive` 归档读取样板**：
   * `src/map/unitsdoo.cpp`、`src/map/w3d.cpp`、`src/map/w3i.cpp`、`src/map/w3r.cpp`、`src/map/w3x_storm.cpp`、`src/map/wpm.cpp`、`src/map/object_mods.cpp`、`src/core/game_controller_internal.h`、`src/core/game_bootstrap_diagnostics.cpp`、`src/scripting/jass_common_env.cpp`、`src/scripting/jass_config_env.cpp` 均绕过 `MpqManager`，直接调用底层 StormLib C API（`SFileOpenArchive` -> `SFileOpenFileEx` -> `SFileGetFileSize` -> `SFileReadFile` -> `SFileCloseFile` -> `SFileCloseArchive`）；
   * `jass_common_env.cpp` 与 `jass_config_env.cpp` 更是逐字重复了 90+ 行完全相同的 `ReadFileFromMapArchive`、`ReadFileFromMpq`、`ReadJassFile` 代码。

2. **`src/map/object_mods.cpp` 与 `src/data/*data.cpp` 存在大量重复的物编叠加与 SLK/TXT 读取循环**：
   * `src/map/object_mods.cpp` 中的 `LoadWar3MapUnitMods`、`LoadWar3MapDestructableMods`、`LoadWar3MapDoodadMods`、`LoadWar3MapItemMods`、`LoadWar3MapAbilityMods`、`LoadWar3MapUpgradeMods`、`LoadWar3MapBuffMods` 7 个函数（共 134 行）完全是同一段 `war3map.<ext> -> war3mapSkin.<ext> -> war3campaign.<ext>` 叠加逻辑的 7 次复制粘贴；
   * `destructabledata.cpp`、`doodaddata.cpp`、`itemdata.cpp`、`upgradedata.cpp`、`abilitydata.cpp`、`unitdata.cpp` 各自手写了 `mpq.ReadFile` + `slk.Load` / `skinTbl.Load` 循环，且 `unitdata.cpp` 的 `loadOverlayEverywhere` 对每个 TXT 文件重复执行了 5 次 `mpq.ReadFile`（共 80 次归档读取）；
   * `abilitydata.cpp` 与 `upgradedata.cpp` 重复定义了 `EqualsNoCase`，而 `slk_merge.h` 的 `TryGetTxtValueCaseInsensitive` 每次比较都构造临时 `ToLowerCopy` 字符串。

3. **`CAbilityData` 缺少内存态注册接口，且 `War3GameModule` 缺少运行时无 MPQ 注入自定义物品/升级/技能数据原型的能力**：
   * `CUnitData` 与 `CDestructableData` 提供了 `Register*` 接口，但 `CAbilityData` 缺少 `RegisterAbility` / `RegisterBuff`，且插件模块无法在无外部 SLK 文件时向 `CSimulation` 动态注入完整的物品（`CItemType`）、科技升级（`CUpgradeType`）与技能数据定义（`CAbilityDefFull`）。

### 3.2 Phase 21 核心架构改造与精简成果

| 重构维度 | 具体改造内容 | 量化收益与通用引擎价值 |
| ----- | ----- | ----- |
| **归档后端彻底封装与 JASS 脚本加载器合并（Stage XX）** | 在 `MpqManager`（`src/resource/mpq_storm.h/.cpp`）中新增 `ReadSingleArchiveFile`、`ReadFromGameArchivesOnly` 与 `ReadJassScriptFile`（同时消重 `MpqManager::ReadFile` 内部的 `war3mapImported\` 归档读取）；重构 `unitsdoo.cpp`、`w3d.cpp`、`w3i.cpp`、`w3r.cpp`、`w3x_storm.cpp`、`wpm.cpp`、`object_mods.cpp`、`game_controller_internal.h`、`game_bootstrap_diagnostics.cpp`、`jass_common_env.cpp`、`jass_config_env.cpp` | **除 `src/resource/mpq_storm.h` 外，全库 10 个外部文件对 `<StormLib.h>` 的直接 `#include` 全部清零（10 -> 0）**；消除 11 处重复的 `SFileOpenArchive` 句柄管理样板；`jass_common_env.cpp` 与 `jass_config_env.cpp` 合计精简 ~135 行重复代码 |
| **物编分层加载器与 SLK/TXT 数据表加载管线消重（Stage XX/YY）** | 在 `src/map/object_mods.cpp` 中提炼 `LoadWar3MapObjectModsWithLayeredFallbacks` 统一承载 7 类物编（`w3u`/`w3b`/`w3d`/`w3t`/`w3a`/`w3q`/`w3h`）；在 `src/data/slk_merge.h/.cpp` 中新增零分配 `EqualsIgnoreCase`、`LoadSlkTableFromMpq` 与 `LoadTxtOverlaysFromMpq`，重构全部 6 个数据表加载器，并将 `unitdata.cpp` 的 `loadOverlayEverywhere` 优化为单次 `mpq.ReadFile` 缓冲复用 | `src/map/object_mods.cpp` 7 个加载函数从 134 行精简至 45 行（缩减 66.4%）；`CUnitData::LoadFromMpq` 的 TXT 归档读取次数从 80 次降至 16 次（减少 80%）；消除 `abilitydata.cpp` 与 `upgradedata.cpp` 的重复字符串工具函数 |
| **`War3GameModule` Version 21 运行时物编数据原型注入（Stage YY）** | 在 `CAbilityData` 上补齐 `RegisterAbility` 与 `RegisterBuff`；将 `War3GameModule` 与 `libwar3_module_plugin.so` 升级至 **Version `21u`**，新增 `RegisterCustomItemArchetype`、`RegisterCustomUpgradeArchetype` 与 `RegisterCustomAbilityDef` | 允许通用引擎宿主、单元测试或自定义 MOD 插件在纯内存态（无需挂载外部 MPQ/SLK 文件）动态注册完整物品、多层科技升级与技能数值表，并与 `CSimulation` 实时升级/背包/施法管线无缝联动 |

## 4. 云端游戏资源验证与单元测试覆盖

结合云端 Google Drive [`WarCraft3`](https://drive.google.com/drive/folders/1sEyljyjTcnwtxQNPEhYC2dVMh4Idit-L) / [`Maps/FrozenThrone`](https://drive.google.com/drive/folders/1sb1fktbnjqbIEjNrKky4l_iWyMfb_wU3) 下的 14 张零售级对战与 [`Scenario`](https://drive.google.com/drive/folders/18rVJURpX4ZHWLumjNUp0ZeXEN-TJOaj-) 地图（含 [`(2)EchoIsles.w3x`](https://drive.google.com/file/d/1d5qijwX3yNOvm9Xej64YpfSTivPlNGff/view?usp=drivesdk)、[`(4)TurtleRock.w3x`](https://drive.google.com/file/d/106f_pFaSePSYhTJTZezXX4NYGotoG1pY/view?usp=drivesdk)、[`(4)TwistedMeadows.w3x`](https://drive.google.com/file/d/1v51wDc9ilk5BtPbEsPUX0mZVWnA3olix/view?usp=drivesdk)、[`(8)GardenOfWar.w3x`](https://drive.google.com/file/d/1LOvXHMDAyjLaLdZlAg4ubMIAcqNi6qrP/view?usp=drivesdk)、[`(6)BlizzardTD.w3x`](https://drive.google.com/file/d/1NK4On-ccrEEzFzRPsTv625yZY5QXl-h8/view?usp=drivesdk)、[`(8)AzerothGrandPrix.w3x`](https://drive.google.com/file/d/1H-h7ZYHa2qdwykkdNkpSaRDdKuNojLAM/view?usp=drivesdk) 等），全量运行 **853 项** 单元与集成测试：

* **Phase 12–20 专项测试（`Stage EE1`–`Stage WW2`）**：36 项全通过。
* **Phase 21 专项测试**：
  * `Stage XX1`: 验证统一归档接口 `MpqManager::ReadSingleArchiveFile`（直接解析云端地图 `war3map.w3i`、`war3map.w3e`、`war3map.wpm`）与 `MpqManager::ReadJassScriptFile`（解析 `Scripts\war3map.j`）及缺失归档安全兜底。
  * `Stage XX2`: 验证 `EqualsIgnoreCase`、`TryGetTxtValueCaseInsensitive` 大小写无关属性查找，以及统一分层物编加载器 `LoadWar3MapObjectModsWithLayeredFallbacks` 在云端场景地图 `(6)BlizzardTD.w3x` 上的物编解析。
  * `Stage YY1`: 验证 `War3GameModule` Version 21 运行时内存态物编原型注入（`RegisterCustomAbilityDef` 注入 3 级自定义技能数值表 `ACs1`、`RegisterCustomItemArchetype` 注入自定义充能物品 `Ics1`、`RegisterCustomUpgradeArchetype` 注入自定义护甲科技 `Rcs1` 并实时作用于英雄护甲）。
  * `Stage YY2`: 验证动态加载 `./libwar3_module_plugin.so`（Version 21）以及云端零售地图 `(2)EchoIsles.w3x`、`(8)GardenOfWar.w3x` 端到端加载与模拟步进。
* **总计 853 / 853 项测试全部通过（100% 通过率，零回归）**。
