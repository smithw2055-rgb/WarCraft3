# Warsmash C++ Recreation Plan

## Project Overview
Goal: Build a Warcraft III-compatible emulator in modern C++ that can load and run the same game data (maps, models, textures, audio, UI, scripts) as the original Warsmash (Java/LibGDX) engine.

Target platforms: Windows, Linux, macOS (desktop)
Rendering: OpenGL 3.3+ or Vulkan (OpenGL preferred for initial prototype)
License: AGPL-3.0 (to match Warsmash)

## High-Level Architecture
```
+-------------------+      +-------------------+      +-------------------+
|   Platform Layer  |      |   Resource System |      |   Game Systems    |
| (Window, Input,  |<---->| (MPQ/CASC, VFS,   |<---->| (Simulation,     |
|  Audio, Timing)  |      |  caching, async) |      |  ECS, AI, Net)    |
+-------------------+      +-------------------+      +-------------------+
          ^                         ^                         ^
          |                         |                         |
          |                         v                         v
          |               +-------------------+      +-------------------+
          |               |   Rendering       |      |   Scripting (JASS)|
          |               |   (Models,        |<---->|   (VM, natives)   |
          |               |   Textures,       |      +-------------------+
          |               |   Shaders, UI)    |
          |               +-------------------+
          |                         ^
          |                         |
          +-------------------------+
                    +-------------------+
                    |   Asset Pipeline  |
                    | (MDX/MDL, BLP/TGA/DDS,
                    |  W3X, SLK/INI, FDF) |
                    +-------------------+
```

## Core Sub-Systems & Responsibilities

| Sub-system | Main Tasks |
|------------|------------|
| **Window & Input** | Create window, handle keyboard/mouse/gamepad, timing |
| **Audio** | Decode & play WAV/FLAC, music, 3D positional sounds |
| **Virtual File System (VFS)** | Mount MPQ, CASC, folders; provide file streams; newest-over-oldest lookup |
| **Model Loader (MDX/MDL)** | Parse binary MDX or text MDL, extract meshes, skeletons, animations, textures, lights, emitters |
| **Texture Loader (BLP/TGA/DDS)** | Read Blizzard BLP (palette + alpha), TGA, DDS; upload to GPU |
| **Map Loader (W3X)** | Read map header, load tileset, pathing, doodads, units, dependencies, custom data |
| **Unit Data (SLK/INI)** | Parse SLK (spreadsheet) and INI (key=value) for unit, upgrade, ability tables |
| **JASS VM** | Execute JASS scripts, expose natives (game API) |
| **Rendering** | Draw terrain (heightmap + textures), models (skinned meshes), UI, particles, selection circles, fog of war |
| **Physics / Pathing** | Ground collision, cliff detection, pathing map (from W3X) |
| **ECS / Game Logic** | Store unit stats, buffs, timers; process orders, ability casts, combat |
| **Networking (optional)** | LAN/Internet multiplayer (replicate commands) |
| **Build System** | Manage dependencies, cross-platform compilation |
| **Testing** | Unit tests for parsers, integration tests for map loading, rendering sanity checks |

## Implementation Roadmap (Phased)

### Phase 0 – Project Setup
- Create repo, CMake configuration
- Add SDL2, GLAD (or GL3W) for OpenGL context, Dear ImGui, stb_image, miniaudio, entt
- Set up basic window + clear color + ImGui demo

### Phase 1 – VFS & Archive Support
- Implement MPQ reader (lookup files, provide `std::istream`-like interface)
- Add CASC support via casc-lib (or defer to later if complex)
- Build a mounting layer that can overlay multiple archives + folders
- Test by loading a loose file from disk and from an MPQ

### Phase 2 – Basic Asset Loaders
- **TGA** (via stb_image) → texture
- **BLP**: write loader that reads palette + alpha, converts to RGBA8
- **DDS**: use stb_image or DirectXTK (if on Windows) or tinyexr
- Verify by loading sample textures and displaying them on a quad

### Phase 3 – Model Loader (MDX/MDL)
- Parse MDX binary format (header, chunks: vertices, normals, UVs, faces, bones, animations, textures, emitters, lights)
- Convert to engine format: mesh (VAO/VBO), skeleton (bone hierarchy + inverse bind poses), animation clips
- Simple shader that renders a model with a single texture
- Test with a known unit model (e.g., Footman.mdl)

### Phase 4 – Map & Terrain Loading
- Parse W3X: map size, tileset, tile ground/cliff layers, pathing map, water, boundaries
- Build terrain mesh (grid of quads) with multi-texture splatting (based on tileset IDs) and cliff geometry
- Implement basic vertex shader that samples up to 4 textures per tile + normal map (if available)
- Load and display a simple map (e.g., Blizzard's tutorial map) with units as billboards

### Phase 5 – Unit Data & SLK/INI
- Load `units.slk`, `upgrades.slk`, `abilities.slk`, `items.slk` (and corresponding INIs if present) into readable tables
- Expose data to ECS components (e.g., `UnitType`, `AbilityDef`)

### Phase 6 – ECS Integration & Basic Game Loop
- Create ECS entities for units, buildings, doodads from map data
- Add components: Transform, MeshInstance, Team, Health, Orders
- Implement simple systems:
  - Movement (follow waypoint, basic obstacle avoidance using pathing map)
  - Rendering (draw all mesh instances)
  - Selection (mouse-pick via ray-cast against terrain + unit bounding boxes)
- UI: ImGui panel showing selected unit's stats

### Phase 7 – JASS VM (Core Natives)
- Design bytecode format for JASS (or reuse existing interpreter)
- Implement core language: variables, functions, control flow, natives (e.g., `GetTriggerUnit`, `CreateUnit`, `IssuePointOrder`)
- Hook natives to ECS (spawn unit, issue order, etc.)
- Test with a simple map that runs a JASS trigger (e.g., "when unit dies, create a unit")

### Phase 8 – Audio & Music
- Load `.wav` and `.flac` from MPQ/CASC via miniaudio
- Implement 3D positional audio for unit attacks/ambient
- Play background music from map's MP3 (if any) or from internal music folder

### Phase 9 – UI / FDF Parser
- Parse FDF (frame definition) files to create UI frames (buttons, text, model portraits)
- Map FDF events to JASS triggers or ECS commands (click, hover)
- Render UI using ImGui or custom quad-based renderer (matching original Warcraft III look)

### Phase 10 – Polish & Features
- Fog of war (height-based + visibility mask from map)
- Day/night cycle & point lights (torches, spell effects)
- Particle system (from MDX emitters) – simple billboard quads
- Network command replication (optional for multiplayer)
- Config file (`warsmash.ini`) to set DataSources, MaxPlayers, video settings

### Phase 11 – Testing, Performance, Packaging
- Unit tests for all parsers
- Performance profiling (GPU/CPU)
- Create build scripts for Windows (VS), Linux (make), macOS (Xcode) via CMake
- Package binary release with sample `warsmash.ini` and a small test map

## Dependencies (Suggested Versions)

| Library | Purpose | License |
|---------|---------|---------|
| SDL2 | Window, input, gamepad, basic audio (fallback) | zlib |
| GLAD / GL3W | OpenGL loader | MIT |
| Dear ImGui | Debug UI, tools | MIT |
| stb_image | Image loading (TGA, BMP, PNG, JPG) | PD/MIT |
| miniaudio | Audio decoding/playback (WAV, FLAC, streaming) | MIT |
| entt | ECS | MIT |
| casc-lib | CASC archive reading | BSD-3 |
| libmpq (custom) | MPQ archive reading | MIT / LGPL (check) |
| sol2 (if using Lua bridge for JASS) | Lua ↔ C++ binding | MIT |
| Catch2 / GoogleTest | Unit testing | Boost / MIT |
| glm | Math (vectors, matrices) | MIT |
| assimp (optional) | Alternative model import pipeline (if MDX plugin written) | BSD |

*All must be AGPL-3.0 compatible or have a compatible exception.*

## Build System (CMake Sketch)

```cmake
cmake_minimum_required(VERSION 3.20)
project(WarsmashCpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Options
option(BUILD_TESTS "Build unit tests" ON)
option(USE_VULKAN "Use Vulkan instead of OpenGL" OFF)

# Dependencies via FetchContent (or vcpkg)
include(FetchContent)

FetchContent_Declare(
  sdl2
  GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
  GIT_TAG release-2.28.0
)
FetchContent_MakeAvailable(sdl2)

# Repeat for glad, imgui, stb, miniaudio, entt, casc-lib, mpq, sol2, etc.

add_executable(warsmash
  src/main.cpp
  src/platform/sdl_window.cpp
  src/resource/vfs.cpp
  src/resource/mpq_archive.cpp
  src/resource/casc_archive.cpp
  src/asset/model_mdx.cpp
  src/asset/texture_blp.cpp
  src/asset/map_w3x.cpp
  src/data/slk_parser.cpp
  src/data/ini_parser.cpp
  src/scripting/jass_vm.cpp
  src/ecs/systems/*.cpp
  src/render/renderer.cpp
  src/render/shaders/*.cpp
  src/ui/fdf_parser.cpp
  src/ui/imgui_wrapper.cpp
)

target_include_directories(warsmash PRIVATE
  ${SDL2_INCLUDE_DIRS}
  ${GLAD_INCLUDE_DIRS}
  ${IMGUI_INCLUDE_DIRS}
  ${STB_INCLUDE_DIRS}
  ${MINIAUDIO_INCLUDE_DIRS}
  ${ENTT_INCLUDE_DIRS}
  ${CASC_INCLUDE_DIRS}
  ${MPQ_INCLUDE_DIRS}
  ${SOL2_INCLUDE_DIRS}
)

target_link_libraries(warsmash PRIVATE
  SDL2::SDL2
  glad
  imgui
  stb
  miniaudio
  entt::entt
  casc-lib
  mpq
  sol2::sol2
  ${OPENGL_LIBRARIES}
  ${CMAKE_THREAD_LIBS_X_INIT}
)

if(BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
```

## Testing Strategy

- **Unit Tests**: 
  - MPQ/CASC file listing & extraction
  - MDX chunk parsing (vertex counts, animation frames)
  - BLP → RGBA conversion
  - W3X header + tile data extraction
  - SLK/INI → structured data
  - JASS VM op-codes and native calls

- **Integration Tests**:
  - Load a known map, verify terrain mesh vertex count matches map size
  - Spawn a unit from map data, check its transform and team component
  - Play a simple JASS trigger and assert expected ECS changes (e.g., unit created on death)

- **Regression / Visual Tests** (optional):
  - Render a frame and compare against a reference screenshot (using perceptual diff tools)
  - Useful for ensuring shader/model changes don't break visual fidelity

- **Performance Benchmarks**:
  - FPS with a moderate-sized map (e.g., 64x64) and 50 units
  - Memory usage of loaded assets (textures, models)

## Risks & Mitigations

| Risk | Impact | Mitigation |
|------|--------|------------|
| **MDX/MDL format nuances** (undocumented chunks) | Models may render incorrectly or crash | Start with existing Warsmash/Retera Model Studio source as reference; write extensive unit tests against known good models; fallback to assimp if needed |
| **CASC parsing complexity** | Delayed support for newer patches (1.30+) | Implement MPQ-only VFS first; later integrate casc-lib; keep VFS abstraction so adding CASC is a drop-in |
| **JASS language quirks** (globals, natives, handle leaks) | Scripts may behave differently | Focus on a subset of natives needed for melee maps first; incrementally add more; use existing JASS test maps for validation |
| **Rendering differences** (original fixed-function vs modern shaders) | Visuals may look "off" | Replicate the original lighting model (ambient + directional + point lights) and texture combiner logic in shaders; use the same texture addressing (clamp/repeat) and mipmap generation |
| **Legal – asset redistribution** | Engine itself is fine, but users must supply their own WC3 assets | Clearly document that the engine does not ship any Blizzard data; provide instructions on how to point to a legal WC3 install (mirroring Warsmash's `warsmash.ini` approach) |

## Raylib Consideration

Raylib is a viable alternative that simplifies windowing, input, rendering, and audio:

### Advantages
- Unified API replaces SDL2 + GLAD + OpenGL boilerplate
- Built-in math types (Vector3, Matrix, Quaternion)
- Audio support for WAV, OGG, MP3, XM, MOD (FLAC requires manual decoding)
- Shader flexibility via `rlLoadShaderCode`
- Simple integration (single-header option or easy linking)
- Permissive zlib license (AGPL-3.0 compatible)
- Rapid prototyping capabilities

### Considerations
- **FLAC Audio**: No native support - decode to PCM in memory (e.g., with `dr_flac`)
- **Advanced UI**: Basic GUI controls insufficient - pair with Dear ImGui (has raylib bindings) or custom FDF renderer
- **Rendering Control**: Abstracts OpenGL but allows custom shaders - verify performance meets needs

### Recommendation
Yes, use raylib - it reduces dependency complexity and accelerates early milestones. Workarounds for FLAC/UI are straightforward.

## Open Questions for Clarification

1. Target platform(s) - Windows only, or also Linux/macOS?
2. Preferred rendering API - OpenGL (simpler) or Vulkan (future-proof)?
3. Multiplayer interest - LAN/Internet support from start, or focus on single-player/melee maps first?
4. JASS approach - Fresh JASS interpreter, or bridge to existing language (Lua/WebAssembly) and reuse JASS-to-Lua transpiler?
5. Level of fidelity - 1:1 behavior (including bugs) or "clean-room" improvement (fixing known WC3 engine limits)?

---
*Documentation generated from planning discussion. This plan outlines the approach for recreating Warsmash in C++ based on the open-source WarsmashModEngine project.*