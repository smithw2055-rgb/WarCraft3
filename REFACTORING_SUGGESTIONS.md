# Warsmash C++ 重构与改进建议（基于现代游戏开发实践）

对 Warsmash C++ 项目（Warcraft III 兼容引擎的 C++ 重写版）的宏观扫描显示，该项目已经取得了令人瞩目的进展，成功实现了基础的资源加载（MPQ/CASC）、地图解析、JASS 虚拟机绑定、渲染（基于 `sokol_gfx`）和一套游戏逻辑抽象。

目前架构更接近于 2000 年代中后期的经典面向对象（OOP）游戏引擎设计，并带有部分现代组件化思想。为了使项目在未来更好地扩展、提升性能并符合现代引擎（如 Unreal Engine 5、Unity DOTS 或现代自研引擎）的标准，特提出以下从宏观架构到具体子系统的重构建议。

---

## 1. 架构与游戏逻辑 (ECS 与 Data-Oriented Design)

### 当前现状
项目在 `src/ecs/` 目录下实现了游戏逻辑（如 `CUnit`, `CWidget`, `CBuff` 等）。然而，这里的 "ECS" 实际上是一个**经典的面向对象层次结构（OOP）**。`CUnit` 类（见 `unit.cpp`）非常庞大，包含了从寻路、生命回复、光环计算、战斗表现到渲染动画等一切逻辑。数据和行为被紧密地封装在庞大的类实例中。

### 现代改进建议：纯数据驱动的 ECS (Entity-Component-System)
现代游戏引擎（如 Unity DOTS, EnTT 库，或 Unreal 的 MassEntity）全面采用数据驱动设计（Data-Oriented Design, DOD）。
*   **重构为结构体数组 (SoA)**：不要让 `CUnit` 成为一个拥有上百个字段和虚函数的巨型类。引入真正的 ECS 框架（例如现成的 `EnTT` 或 `flecs`）。
*   **组件拆分 (Components)**：将 `CUnit` 拆分为纯数据的组件。例如：`PositionComponent`, `HealthComponent`, `MovementComponent`, `AuraReceiverComponent`, `AnimationStateComponent`。
*   **系统分离 (Systems)**：将 `unit.cpp` 中的 `Update` 函数拆分为多个独立的系统。例如 `MovementSystem`, `HealthRegenSystem`, `AuraSystem`, `CombatSystem`。系统按顺序迭代拥有特定组件的实体。
*   **收益**：极大地提高 CPU 缓存命中率（Cache Coherency），使得状态更新逻辑更容易并行化（多线程）。

## 2. 并发与多线程 (Task Graph / Job System)

### 当前现状
项目（如 `GameController::StepSimulation` 和 `Render`）主要采用单线程的大循环机制。逻辑更新（Simulation）和渲染（Rendering）交错或顺序执行，没有明显的并行计算任务分配。

### 现代改进建议：引入 Job System (Task Graph)
现代游戏引擎通过任务图（Task Graph）或 Job System 榨干多核 CPU 的性能。
*   **分离主循环与工作线程**：引入基于无锁队列的线程池和任务依赖图（可参考 `cpp-taskflow` 或自己实现一个基于 fiber/微线程的 Job System）。
*   **模拟与渲染解耦**：允许 `Simulation` 在更新第 N 帧时，`Render` 线程正在准备第 N-1 帧的渲染数据（双缓冲状态）。
*   **并行化系统**：基于 ECS 重构后，如果没有数据竞争，不同的系统（如所有单位的位置更新，和所有单位的生命回复）可以作为并行的 Job 抛给工作线程同时执行。

## 3. 渲染架构 (Render Graph 与 Bindless)

### 当前现状
渲染模块使用 `sokol_gfx`，这是一个优秀的跨平台现代图形包装器。但从 `Renderer::BeginFrame` 和 `GameController::Render` 的组织来看，渲染命令是**立即模式（Immediate）**或在遍历场景时直接提交的（Forward Rendering）。

### 现代改进建议：Render Graph (Framegraph) 与数据驱动渲染
*   **引入 Render Graph (帧图)**：不要在游戏逻辑或场景遍历中直接调用图形 API/Sokol 命令。应该让各个渲染通道（Pass，如 Shadow Pass, G-Buffer Pass, Deferred Lighting Pass, UI Pass）声明它们的输入和输出资源。Render Graph 负责分析依赖，自动处理资源屏障（Barriers，如果未来用 Vulkan/DX12）和显存复用。
*   **Clustered Forward Shading 或 Deferred Shading**：Warcraft 3 依赖大量的点光源（法术特效）和贴花（Splats）。为了支持大量动态光源而不打断 Batch，建议引入 Clustered Shading 或 Tiled Deferred Shading。
*   **Bindless Resources (如果硬件允许)**：通过将所有的贴图放入一个全局数组，并在顶点/实例数据中传递贴图索引，可以极大地减少 Draw Call 之前的绑定开销，实现按材质甚至无材质的合批（GPU-driven rendering）。

## 4. 资产与资源流式加载 (Asset Pipeline & Asynchronous IO)

### 当前现状
资源（MPQ/CASC）读取大部分是同步阻塞的。`mapLoader_` 在加载阶段虽然有进度概念，但在实际运行中（如 `JASS` 脚本动态请求特效模型）可能会导致主线程卡顿。资源的解析（如 MDX 到 GPU Buffer）也与加载耦合。

### 现代改进建议：烘焙流水线与异步流式加载
*   **资产烘焙 (Asset Cooking/Baking)**：MDX 等暴雪原始格式在运行时解析成本较高。现代引擎通常有离线的 "Cook" 步骤，将原始模型/纹理转换为直接对齐 GPU 内存结构的二进制格式，运行时通过内存映射 (Memory Mapped IO) 直接读取。
*   **纯异步 VFS (Virtual File System)**：引入后台 IO 线程。当游戏请求 `UI\\HumanClick.wav` 时，返回一个 `AssetHandle`（此时处于 Loading 状态），并在后台完成解压和反序列化。主线程遇到未加载完的资源时不阻塞，而是渲染占位符（Placeholder）或跳过该帧的渲染。
*   **资源引用计数与垃圾回收**：目前的资源生命周期管理较弱，应实现统一的 `ResourceManager`，基于 `std::shared_ptr` 或侵入式引用计数自动卸载长久不用的资源。

## 5. 内存管理与缓存机制

### 当前现状
代码中频繁使用 `std::make_unique`、`std::vector` 的动态增删，以及在热路径（如伤害计算、寻路）中的动态内存分配。

### 现代改进建议：自定义内存分配器 (Custom Allocators)
*   **对象池 (Object Pools)**：对于高频创建和销毁的对象（如特效、抛射物 `Projectile`、伤害事件结构、UI 浮字），使用预分配的对象池。
*   **帧分配器 (Frame / Linear Allocator)**：在每帧的渲染或物理计算中，有大量只存活一帧的临时数据。引入 `LinearAllocator`，每帧开始时指针清零，分配时只需指针累加，极大地消除 `new/delete` 带来的开销和内存碎片。

## 6. 构建、测试与依赖管理

### 现代改进建议
*   **依赖管理现代化**：虽然 CMake 支持 FetchContent，但随着第三方库增多，建议使用 `vcpkg` 或 `Conan` 作为正式的包管理方案（特别是在支持跨平台时）。
*   **持续集成与自动化测试 (CI/CD)**：目前虽然有 `BUILD_TESTS`，但在模拟器引擎中，回归测试至关重要。建议增加**无头（Headless）运行模式**下的确定性测试（Deterministic Testing），即给入一系列相同的输入，在特定帧数的模拟后，对整个游戏状态（哈希值）进行断言，以确保逻辑重构不会破坏原版（零售版）的准确性。

---

## 总结路线图 (Refactoring Roadmap)

如果您计划实施这些重构，建议采用渐进式的策略：

1.  **第一步（基础设施）**：引入帧分配器（Frame Allocator），消除热循环（如 Update, JassVM 执行）中的堆分配。
2.  **第二步（解耦系统）**：逐步将 `CUnit` 庞大的 `Update` 函数拆分。不需要马上引入完整的 ECS 框架，但可以将数据结构从类内部剥离出来成为 `Structs`。
3.  **第三步（异步化）**：将 IO（读取 MPQ/CASC 文件）移至后台线程，实现 `Future/Promise` 或回调机制处理资源加载。
4.  **第四步（现代渲染）**：重构 Renderer 层，引入 Framegraph 概念，将场景的遍历与图形命令的提交解耦。
