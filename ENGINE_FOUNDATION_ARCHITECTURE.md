# 现代化游戏引擎底座抽象与精简重构设计文档 (Phase 12 – Phase 22)

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
* **Phase 22**：**完成 `CBuff` 8-bit 紧凑状态位域（`BuffStateFlag`）与统一战斗修正虚接口解耦（彻底移除 `src/ecs/unit.cpp` 与 `src/scripting/*` 对 `ecs/buffs.h` 的直接依赖）、`CAbility` 完整多态虚契约补齐与技能间残留跨头文件耦合清零（移除 `spells22.cpp`、`spells29.h/.cpp`、`ability_hero.cpp`、`ability_queue.cpp` 的跨技能 `#include`），消除全库 `src/` 下全部 44 处 `dynamic_cast<CBuff*>` / `dynamic_cast<CAbility*>` RTTI 开销（44 -> 0），并将 `War3GameModule` 升级至 Version 22（新增运行时动态 Buff 工厂与声明式周期/属性/修正 Buff 注册接口 `RegisterCustomBuffCreator`、`RegisterCustomBuffSpec`、`CreateTimedLifeBuff`、`CreateBuffByCode`、`TryApplyCustomBuffByCode`）**。

## 1. 通用化引擎底座架构抽象（`src/engine/`）

为了将本项目从“单一硬编码的魔兽争霸3模拟器”演进为“可复用、可插拔的通用 RTS/RPG 现代游戏引擎”，Phase 12–22 抽离了零业务耦合的 `src/engine/` 引擎内核层，将底层服务与上层游戏规则（`IGameModule` + `ISimulationModule`）彻底解耦：

| 引擎底座模块 | 核心职责与设计模式 | 对标现代引擎参考 |
| ----- | ----- | ----- |
| `engine::ISubsystem` & `engine::SubsystemRegistry` (`src/engine/engine_subsystem.h`) | 定义 9 阶段确定性生命周期总线（`Platform(0) -> Jobs(50) -> Resource(100) -> Audio(200) -> Scripting(300) -> Simulation(400) -> Presentation(500) -> Render(600) -> UI(700) -> Telemetry(800)`），提供 `O(1)` 类型索引服务定位器（`Register<T>` / `TryGet<T>`）、按阶段升序初始化与逆序安全销毁 | Unity `SubsystemManager` / Godot `Server` 注册表 |
| `engine::FixedTimestepStepper` (`src/engine/fixed_timestep_stepper.h`) | 将固定频率确定性模拟步进（如 20Hz RTS 帧同步或 60Hz 物理步进）与高刷显示器渲染帧率（60/144/240Hz）彻底解耦；内置死亡螺旋钳制（`maxSubStepsPerFrame`）并自动计算 `[0, 1]` 帧间插值因子 `interpolationAlpha` | Godot `MainLoop` 物理插值分步器 / Unity `FixedUpdate` |
| `engine::IGameModule` (`src/engine/game_module.h`) | 定义通用引擎底座与具体游戏业务模块（如 `War3GameModule`、塔防/MOBA/RPG 模块或无头回归测试宿主）的标准生命周期契约（`OnRegisterSubsystems`、`OnInit`、`OnFixedTick`、`OnFrameUpdate`、`OnRender`、`OnShutdown`） | Godot `MainLoop` / Unity `World` 业务插件 |
| `engine::ISimulationModule` & `engine::NullSimulationModule` (`src/engine/simulation_module.h`) | **\[Phase 13–22 核心\]** 抽象 `CSimulation` 通用 ECS 世界容器、`CUnit` 通用实体、`CPlayer` 通用玩家状态、`CCombatSystem` 通用战斗计算器及 `MeleeAIController` 战术 AI 与具体游戏玩法规则的完整边界契约：涵盖技能目录挂载/卸载、单位原型技能装配与数据绑定、实体生命周期钩子、科技树等效层级解析、科技升级实时属性加成引擎、经济维护费税率、胜负与残局判定、攻击/护甲克制倍率与护甲减伤曲线、战术 AI 托管与种族建造规划发现（`OnDiscoverAIRacePlan`）、伤害前置/后置修正、命中被动触发、击杀赏金与英雄经验分配、空闲自动施法与动态注册（`GetAbilityAutocastKind` / `RegisterAutocastAbility`）、物品属性路由、战役跨图英雄快照恢复、运行时声明式法术/光环/命令动态注册（`RegisterCustomSpellDescriptor` / `RegisterCustomAura` / `RegisterCustomOrder`）、建筑建造网格吸附与占地碰撞校验（`OnSnapBuildPosition` / `IsBuildLocationObstructed`）、运行时内存态物编原型注入（`RegisterCustomItemArchetype` / `RegisterCustomUpgradeArchetype` / `RegisterCustomAbilityDef`），以及 **\[Phase 22 新增\] 运行时动态 Buff 工厂与生命周期绑定契约（`CreateTimedLifeBuff` / `CreateBuffByCode` / `TryApplyCustomBuffByCode` / `RegisterCustomBuffCreator` / `RegisterCustomBuffSpec`）**。挂载 `NullSimulationModule` 时 `CSimulation` 即刻进入 **Pure Engine Mode（纯净通用引擎模式）** | Godot `GDExtension` 玩法扩展 / Unity `Authoring & Rule System` |
| `engine::DynamicModuleLoader` (`src/engine/dynamic_module_loader.h/.cpp`) | **\[Phase 13 新增\]** 统一管理内置模块工厂（`RegisterBuiltInModule`）与操作系统原生共享库（`.so` / `.dll`）运行时动态加载（`LoadSharedLibrary`、`UnloadSharedLibrary`、`HotReloadSharedLibrary`），基于 POSIX `dlopen`/`dlsym`/`dlclose`（及 Windows `LoadLibraryA`/`GetProcAddress`）绑定 C ABI 导出入口，并通过 `shared_ptr` 自定义析构器安全托管跨 DLL 边界实例与代数（Generation）追踪 | Godot `GDExtensionLoader` / Unreal `FModuleManager` |
| `engine::EngineKernel` (`src/engine/engine_kernel.h/.cpp`) | 统一收拢七大核心引擎服务器（`JobSystem`、`ResourceServer`、`AudioServer`、`RenderServer + RenderGraph`、`InputActionMap`、`ScriptHostRouter`、`EngineProfiler`），支持**独立自治模式（`BootstrapStandardSubsystems`）**与**非侵入宿主绑定模式（`BindExternalServices`）**双轨运行 | Godot `OS + Engine` / Unity `PlayerLoop` |

## 2. Phase 13–21 核心解耦与精简里程碑回顾

* **Phase 13（`CSimulation` 单位/技能体系插件化）**：移除 `src/ecs/csimulation.cpp` 全部 14 个具体魔兽技能 `#include`，`src/ecs/ability_factory.cpp` 缩减 76%，支持 `libwar3_module_plugin.so` 动态加载与热重载。
* **Phase 14（`CPlayer` 科技树/升级与胜负判定插件化）**：封装四族科技树等效推导、`UpgradeData.slk` 实时属性升级引擎、维护费税率与对战/同盟胜负及残局暴露判定。
* **Phase 15（`CCombatSystem` 克制矩阵与 `MeleeAIController` 战术 AI 插件化）**：抽离 7×8 攻击/护甲克制表、0.06 护甲减伤曲线与战术 AI 护甲克制集火评分。
* **Phase 16（`CUnit` 零技能头文件耦合与 `UnitStateFlag` 位域压缩）**：移除 `src/ecs/unit.cpp` 全部 10 个具体魔兽技能头文件（`unit.cpp` 缩减 40.7%），将 `CUnit` 16 个布尔标志压缩为 2 字节 `uint16_t` 位域（缩减 87.5%）。
* **Phase 17（`src/ecs/` 与 `src/ecs/behaviors/*` 零具体技能头文件解耦 + 8 大声明式法术原语）**：`src/ecs/` 顶层及 `src/ecs/behaviors/*` 对具体技能头文件的 14 处 `#include` 清零；精简 18 个法术文件并支持 `RegisterCustomSpellDescriptor`。
* **Phase 18（`CBuffCloneable` CRTP 消重 + `CAbility`/`CBuff` 位域压缩 + 动态自动施法注册）**：消除全部 48 个 `CBuff` 派生类的手写 `Clone()`（`src/ecs/buffs.cpp` 从 1,257 行降至 567 行，缩减 54.9%），压缩 `CAbility`/`CBuff`/`CAbilitySpellBase` 内存占用 20%–40%，并新增 `RegisterAutocastAbility`。
* **Phase 19（`src/scripting/`、`src/ui/`、`src/core/` 26 处具体技能头文件清零与 JASS Native 绑定精简）**：`src/ui/`（10 -> 0）、`src/core/`（9 -> 0）、`src/scripting/`（7 -> 0）共 26 处具体技能头文件 `#include` 全部清零；提炼 `ResolveJassUnit`、`FindHeroAbility`、`FindUnitInventory`、`FindUnitShop`、`FindAllWorldShops`，并将 `War3GameModule` 升级至 Version 19（新增 `OnSnapBuildPosition` 与 `IsBuildLocationObstructed`）。
* **Phase 20（全库残留跨技能头文件彻底清零 + 声明式光环引擎 + `COrder` `O(1)` 直接值访问与动态命令注册表）**：移除 `map_loader.cpp`、`melee_ai.cpp` 及 6 个技能实现文件内部的跨技能头文件 `#include`；将 `src/ecs/abilities/auras.cpp` 从 192 行精简至 68 行（缩减 64.6%）并新增 `RegisterCustomAura`；消除命令分发热路径上的 `dynamic_cast<COrder*>` 并新增 `RegisterCustomOrder`。
* **Phase 21（归档文件系统封装、10 处 `<StormLib.h>` 外部依赖清零、SLK/TXT/物编分层加载消重与 `War3GameModule` Version 21 内存态物编注入）**：除 `src/resource/mpq_storm.h` 外全库 10 个外部文件对 `<StormLib.h>` 直接依赖清零；`object_mods.cpp` 7 个物编加载函数合并精简 66.4%，`CUnitData::LoadFromMpq` TXT 归档读取次数减少 80%；新增 `RegisterCustomItemArchetype`、`RegisterCustomUpgradeArchetype`、`RegisterCustomAbilityDef`。

## 3. Phase 22：`CBuff` 8-bit 状态位域与战斗修正虚契约、`CAbility` 零 RTTI 多态接口补齐、全库 44 处 `dynamic_cast` 清零与 `War3GameModule` Version 22 动态 Buff 引擎

### 3.1 诊断与精简目标

在完成 Phase 21 后，对 `src/ecs/`、`src/scripting/`、`src/ecs/abilities/` 与 `src/game/war3/war3_game_module.cpp` 的深度审计发现了三处核心耦合与运行时 RTTI 冗余：

1. **`src/ecs/unit.cpp` 与 `src/scripting/` 仍直接 `#include "ecs/buffs.h"` 并高频执行 `dynamic_cast<CBuff*>`**：
   * `src/ecs/unit.cpp` 为了判断单位是否处于召唤物限时生命周期（`CBuffTimedLife`）、妖术变形（`CBuffHex`）、疾风步隐身（`CBuffWindWalk`）、沉默（`CBuffSilence`）或催眠（`CBuffSleep`），直接包含了包含 48 个具体魔兽 Buff 类的 `ecs/buffs.h`，并在 `GetSummonRemainingTime`、`IsHexed`、`BreakStealthOnAttack`、`CanCastSpells`、`WakeFromSleepOnDamage` 中执行了 7 次 `dynamic_cast<CBuff*>`；
   * `src/scripting/natives/jass_natives_units.cpp` 与 `jass_natives_triggers.cpp` 也直接 `#include "ecs/buffs.h"` 并通过 `new CBuffTimedLife` 和 `dynamic_cast<CBuffTimedLife*>` 处理召唤物限时生命；
   * `src/game/war3/war3_game_module.cpp` 在战斗伤害与命中判定热路径（`OnComputeAttackerMissChance`、`OnModifyIncomingDamage`、`OnPostAttackDamageApplied`）中每次普攻/受击都遍历攻击者与受击者的 Buff 列表，执行多达 9 种 `dynamic_cast<const CBuff*>`（`CBuffCurse`、`CBuffHaze`、`CBuffBerserk`、`CBuffDefend`、`CBuffHowl`、`CBuffDamageMod`、`CBuffThorns`、`CBuffLifesteal`、`CBuffWindWalk`）。

2. **技能系统（`src/ecs/abilities/`）与 `war3_game_module.cpp` 仍残留 28 处 `dynamic_cast<CAbility*>` 及 5 处跨技能头文件耦合**：
   * `spells29.h` 直接 `#include "ecs/abilities/spells4.h"`（仅为了让 `CAbilityDrunkenBrawler` 继承 `CAbilityCriticalStrike`），`spells29.cpp` 直接 `#include "ability_harvest.h"` 与 `"ability_gold_mine.h"`，`spells22.cpp` 直接 `#include "spells21.h"`，`ability_hero.cpp` 直接 `#include "ability_spell_base.h"`，`ability_queue.cpp` 直接 `#include "spells12.h"`；
   * `war3_game_module.cpp` 在单位原型技能装配、自动施法扫描與攻击命中被动触发（重击 `AHbh`、分裂攻击 `ANca`、粉碎 `Awar`、尖刺外壳 `AUts`）中执行了 16 次 `dynamic_cast<CAbility*>`。

3. **缺少运行时动态 Buff 工厂与自定义 Buff 注册机制**：
   * 虽然 Phase 17–21 已经支持动态注册法术、光环、命令与物编原型，但通用引擎宿主或 JASS 脚本（如 `UnitApplyTimedLife`、`UnitAddAbility('Bxxx')`）仍无法在不修改 C++ 源码的情况下注册并挂载带周期伤害/治疗、属性加成或战斗修正的自定义 Buff。

### 3.2 Phase 22 核心架构改造与精简成果

| 重构维度 | 具体改造内容 | 量化收益与通用引擎价值 |
| ----- | ----- | ----- |
| **`CBuff` 8-bit 紧凑状态位域与零开销战斗修正虚契约（Stage ZZ1）** | 将 `src/ecs/buff.h` 的 `BuffStateFlag` 扩展至完整的 8-bit 位域（`IS_POSITIVE`、`PERIODIC`、`SUSPENDED`、`TIMED_LIFE`、`HEX`、`SILENCE`、`SLEEP`、`STEALTH`），**保持 `sizeof(CBuff) == 40` 字节零膨胀**；在 `CBuff` 基类上新增 `O(1)` 虚修正契约（`GetMissChance`、`GetDamageDealtFactor`、`GetAttackDealtAdd`、`GetWeaponDamageTakenMultiplier`、`GetAttackTypeDamageTakenMultiplier`、`GetReflectPercent`、`GetStealPercent`、`GetBackstabDamage`、`GrantsEthereal`、`GrantsMagicImmune`） | **彻底移除 `src/ecs/unit.cpp`、`src/scripting/natives/jass_natives_units.cpp`、`src/scripting/natives/jass_natives_triggers.cpp` 对 `ecs/buffs.h` 的直接 `#include`（3 -> 0）**；消除 `unit.cpp`（7 处）、`buffs.cpp`（3 处）、`jass_natives_triggers.cpp`（1 处）与 `war3_game_module.cpp`（9 处）共 **20 处 `dynamic_cast<CBuff*>`（20 -> 0）** |
| **`CAbility` 完整多态虚契约补齐与跨技能头文件清零（Stage ZZ2）** | 在 `src/ecs/ability.h` 中新增 `AbilityStateFlag::GENERIC_PLACEHOLDER` 位域标志（**保持 `sizeof(CAbility) == 24` 字节零膨胀**），并在 `CAbility` 基类补齐训练/研究/升级配置、建造数据绑定、载具容量与装卸载、金矿矿工绑定、被动暴击/重击/分裂/反弹虚访问器；重构 `CAbilityDrunkenBrawler` 直接继承 `CAbilitySpellBase` | **彻底移除 `spells22.cpp`、`spells29.h`、`spells29.cpp`、`ability_hero.cpp`、`ability_queue.cpp` 共 6 处跨技能头文件 `#include`（6 -> 0）**；消除 `src/ecs/abilities/`（8 处）与 `war3_game_module.cpp`（16 处）共 **24 处 `dynamic_cast<CAbility*>`（24 -> 0）**；**全库 `src/` 下 `dynamic_cast<CBuff*>` 与 `dynamic_cast<CAbility*>` 总数从 44 处彻底清零（44 -> 0）** |
| **`War3GameModule` Version 22 动态 Buff 工厂与声明式 `CustomBuffSpec` 引擎（Stage AAA）** | 在 `engine::ISimulationModule` 与 `war3::War3GameModule`（升级至 **Version `22u`**）中新增 `CreateTimedLifeBuff`、`CreateBuffByCode`、`TryApplyCustomBuffByCode`、`RegisterCustomBuffCreator`、`RegisterCustomBuffSpec`、`UnregisterCustomBuff`、`HasCustomBuff`；打通 JASS `UnitApplyTimedLife` 与 `UnitAddAbility` / `AddUnitAbility` 对自定义 Buff 四字符码的自动识别与挂载 | 允许通用引擎宿主、MOD 插件与 JASS 脚本在运行时通过 Lambda 工厂或声明式 `CustomBuffSpec`（支持周期生命/魔法跳字、护甲/攻击力/攻速/移速修正、失误率/增减伤/反伤/吸血/破隐背刺修正，以及眩晕/沉默/妖术/睡眠/隐身/虚无/魔免状态位）动态创建并施加任意自定义 Buff |

## 4. 云端游戏资源验证与单元测试覆盖

结合云端 Google Drive [`WarCraft3`](https://drive.google.com/drive/folders/1sEyljyjTcnwtxQNPEhYC2dVMh4Idit-L) / [`Maps/FrozenThrone`](https://drive.google.com/drive/folders/1sb1fktbnjqbIEjNrKky4l_iWyMfb_wU3) 下的 14 张零售级对战与 [`Scenario`](https://drive.google.com/drive/folders/18rVJURpX4ZHWLumjNUp0ZeXEN-TJOaj-) 地图（含 [`(2)EchoIsles.w3x`](https://drive.google.com/file/d/1d5qijwX3yNOvm9Xej64YpfSTivPlNGff/view?usp=drivesdk)、[`(4)TurtleRock.w3x`](https://drive.google.com/file/d/106f_pFaSePSYhTJTZezXX4NYGotoG1pY/view?usp=drivesdk)、[`(4)TwistedMeadows.w3x`](https://drive.google.com/file/d/1v51wDc9ilk5BtPbEsPUX0mZVWnA3olix/view?usp=drivesdk)、[`(8)GardenOfWar.w3x`](https://drive.google.com/file/d/1LOvXHMDAyjLaLdZlAg4ubMIAcqNi6qrP/view?usp=drivesdk)、[`(2)TerenasStand.w3x`](https://drive.google.com/file/d/1gKKaTaFQOl2PH7RyuWXHjZyFcIdP_L1o/view?usp=drivesdk)、[`(8)PlainsOfSnow.w3x`](https://drive.google.com/file/d/117l6pBeW4STXg1QMn5ITCeJoH5h_GUbg/view?usp=drivesdk)、[`(8)LastManStanding.w3x`](https://drive.google.com/file/d/1wRUzV7Dcgwy_v82LPUFuRPxucm_8fLeg/view?usp=drivesdk)、[`(8)TwilightRuins.w3x`](https://drive.google.com/file/d/1Ab0PR5oLK5fWizsZmTDmqHzGkoovJ3uo/view?usp=drivesdk)、[`(6)BlizzardTD.w3x`](https://drive.google.com/file/d/1NK4On-ccrEEzFzRPsTv625yZY5QXl-h8/view?usp=drivesdk)、[`(8)AzerothGrandPrix.w3x`](https://drive.google.com/file/d/1H-h7ZYHa2qdwykkdNkpSaRDdKuNojLAM/view?usp=drivesdk) 等），全量运行 **857 项** 单元与集成测试：

* **Phase 12–21 专项测试（`Stage EE1`–`Stage YY2`）**：40 项全通过。
* **Phase 22 专项测试**：
  * `Stage ZZ1`: 验证 `sizeof(CBuff) == 40` 紧凑布局、8-bit `BuffStateFlag` 状态位域（`TIMED_LIFE`、`HEX`、`SILENCE`、`SLEEP`、`STEALTH`）以及 `CBuff` 战斗修正虚函数契约（失误率、暴走/防御减伤、恐怖嚎叫攻击削弱、法术/魔法/穿刺减伤、荆棘反伤、吸血、疾风步破隐背刺）。
  * `Stage ZZ2`: 验证 `sizeof(CAbility) == 24` 紧凑布局、`IsGenericPlaceholder()` 未知技能标记、被动战斗技能（重击 `AHbh`、分裂 `ANca`、粉碎 `Awar`、尖刺外壳 `AUts`）虚访问器，以及金矿与采矿技能在零 `dynamic_cast` 下的绑定与矿工进出队列。
  * `Stage AAA1`: 验证 `War3GameModule` Version 22 动态 Buff 工厂（`CreateTimedLifeBuff`、`CreateBuffByCode`）、声明式 `RegisterCustomBuffSpec`（注册带护甲/攻击加成、反伤、吸血与周期治疗的自定义 Buff `BCs1` 并与 `CCombatSystem` 实时联动），以及通过 `UnitAddAbility` 直接向单位施加自定义沉默周期伤害 Buff `BCs2`。
  * `Stage AAA2`: 验证动态加载 `./libwar3_module_plugin.so`（Version 22）以及云端零售与场景地图 `(2)EchoIsles.w3x`、`(4)TurtleRock.w3x`、`(6)BlizzardTD.w3x` 端到端加载与模拟步进。
* **总计 857 / 857 项测试全部通过（100% 通过率，零回归）**。
