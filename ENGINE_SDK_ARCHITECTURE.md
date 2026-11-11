# Engine SDK & Dual-Project Plugin Architecture

## Repository Layout

```text
.
├── engine/                         # 100% game-agnostic, header-only C++17 foundation (/GR-, -fno-rtti)
│   ├── core/                       # TagId, constexpr FNV-1a StaticTypeId, ProjectManifest (JSON & INI)
│   ├── resource/                   # IVfsArchiveProvider, VfsMountRegistry, FormatLoaderRegistry, TableSchemaBinder
│   ├── render/                     # IRhiBackend, NullRhiBackend, StandardRenderPassPipeline, TerrainOverlayState
│   ├── ui/                         # FrameKind bitmask cast, HudViewModel & HudViewBinder (IHudViewListener)
│   └── world/                      # WidgetKind, BinaryWriter/BinaryReader, DeterministicSaveArchive ("ESAV"), ISimulationExtension
├── projects/                       # Pluggable game ruleset modules & project manifests
│   ├── war3/                       # war3.project.json, MpqArchiveProvider, War3SaveContributor, War3 render adapters
│   └── minimal_rts/                # project.json, data/units.json, MinimalRtsModule (zero Warcraft dependencies)
├── apps/                           # Generic command-line & host executables
│   ├── engine_player.cpp           # Plugin-driven simulation host (--project <manifest> --ticks <N>)
│   └── engine_asset_cli.cpp        # Unified VFS / MPQ manifest, glob listing & file probing CLI
├── src/                            # Full Warsmash runtime, ECS, JASS bytecode VM, FDF UI, AudioServer & Sokol RHI
├── tools/                          # Offline asset analysis utilities (resource_auditor, extract_ui_assets, terrain_analyzer)
└── tests/                          # warsmash_tests (full regression suite) & warsmash_rtti_free_tests (/GR-, -fno-rtti gate)
```

## CMake Build Targets

| Target | Kind | Zero-RTTI (`/GR-` / `-fno-rtti`) | Role |
| :--- | :--- | :--- | :--- |
| `engine_foundation` | `INTERFACE` | Yes (`INTERFACE`) | Pure engine headers under `engine/` |
| `war3_module_plugin` | `SHARED` (`.dll`/`.so`) | Yes | Dynamically loadable WarCraft III v33 simulation & VFS plugin |
| `minimal_rts_module` | `SHARED` (`.dll`/`.so`) | Yes | Dynamically loadable MinimalRTS simulation plugin |
| `engine_player` | `EXECUTABLE` | Yes | Generic host loading `war3_module_plugin` or `minimal_rts_module` from manifest |
| `engine_asset_cli` | `EXECUTABLE` | Yes | Multi-subcommand VFS & MPQ archive inspector (`manifest`, `list`, `probe`) |
| `warsmash` | `EXECUTABLE` | Yes | Windowed OpenGL/Sokol WarCraft III client (`BUILD_APP=ON`) |
| `warsmash_rtti_free_tests` | `EXECUTABLE` | Yes | Hardware-enforced Zero-RTTI production + Stage 1–6 test gate |
| `warsmash_tests` | `EXECUTABLE` | Mixed (legacy spell tests) | Full 900+ unit, JASS, deterministic lockstep & Stage 1–6 test suite |
| `resource_auditor` / `extract_ui_assets` / `terrain_analyzer` | `EXECUTABLE` | Yes | Offline asset & terrain diagnostic tools (`BUILD_APP=ON`) |

## Running Both Projects via `engine_player`

```bash
# Default project: WarCraft III (projects/war3/war3.project.json -> war3_module_plugin)
./build/engine_player --ticks 60

# Second verification project: Minimal Sci-Fi RTS (projects/minimal_rts/project.json -> minimal_rts_module)
./build/engine_player --project projects/minimal_rts/project.json --ticks 100
```
