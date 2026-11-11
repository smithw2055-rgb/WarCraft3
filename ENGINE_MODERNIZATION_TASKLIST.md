# Warsmash Engine Modernization Roadmap & Stage 0–6 Verification Record (v2)

## Design Principles
1. **Clean Baseline Fidelity (`bcc34f2`)**: Preserves 100% of `bcc34f2` multi-line formatting, algorithmic comments, Windows `vcpkg_installed` toolchain support, and Phase 32 `WorldPresentationSubsystem` / `OnResolveUnitVisualScaleAndTintForUnit` runtime hooks (zero mechanical line compaction).
2. **Game-Agnostic Engine Foundation (`engine/`)**: Zero Warcraft-specific identifiers, zero `sokol_gfx.h` / `StormLib.h` headers, and zero C++ RTTI (`dynamic_cast` / `typeid`) across all `engine/` headers, enforced via `/GR-` on MSVC and `-fno-rtti` on GCC/Clang.
3. **True Runtime Integration (`src/` + `projects/` + `apps/`)**: Every Stage 1–6 abstraction (`TagId`, `StaticTypeId`, `ProjectManifest`, `VfsMountRegistry`, `FormatLoaderRegistry`, `TableSchemaBinder`, `IRhiBackend`, `StandardRenderPassPipeline`, `HudViewModel`/`IHudViewListener`, `DeterministicSaveArchive`, and `TilesetEnvironmentProfile`) is actively wired into `src/` runtime subsystems and verified end-to-end.

## Stage Checklist (Stages 0–6)

### Stage 0 — Redundancy Elimination & Directory Hygiene
- [x] Relocated third-party `stb_image.h` and `stb_image_write.h` from `src/asset/` into `third_party/stb/`.
- [x] Removed 30 two-line `src/ecs/abilities/{spells*.h, auras.h}` forwarding headers and consolidated 27 fragmented `tests/test_spells3..29.cpp` files into `tests/test_war3_spells_catalog.cpp`.
- [x] Consolidated duplicate `AudioSystem.cpp` into `AudioServer` + `IAudioBackend`, inlined `SimpleFrame` & `GameState`, consolidated single-purpose probe scripts into `apps/engine_asset_cli.cpp`, and kept `resource_auditor`, `extract_ui_assets`, and `terrain_analyzer` actively built in `CMakeLists.txt`.

### Stage 1 — Core Identifiers, Cross-DLL `StaticTypeId` & Manifest System
- [x] `engine/core/tag_id.h`: `constexpr` FourCC + 32-bit FNV-1a `TagId` interoperating seamlessly with `War3ID`.
- [x] `engine/core/type_id.h`: `constexpr` 64-bit FNV-1a hash over canonical compiler type signatures (`__PRETTY_FUNCTION__` / `__FUNCSIG__`), deterministic across `.exe`, `.dll`, and `-fvisibility=hidden` `.so` boundaries without RTTI.
- [x] `engine/core/project_manifest.h`: Structured `MiniJsonParser` and symmetric quoted INI serializer/parser (`ToJson`/`ParseJson`, `ToIni`/`ParseIni`) preserving Windows backslash paths losslessly.

### Stage 2 — Streaming VFS Overlay, Bounds-Safe Native MPQ & Schema Reflection
- [x] `src/resource/mpq_storm.{h,cpp}`: Preserves full `StormLib` path when available and provides a streaming `FILE*`-backed `NativeMpqArchive` fallback (`WITHOUT_STORMLIB=1`) with power-of-two hash validation, `blockSizeShift <= 15` divide-by-zero protection, strict `srcLen == dstLen` over-read prevention, `0x02` zlib / `0x08` PKWARE DCL (`MPQ_FILE_IMPLODE`) / `0x10` BZip2 support, and `MpqManager::ListAllFiles(pattern)` glob filtering.
- [x] `engine/resource/vfs_archive.h`, `format_loader.h` & `projects/war3/mpq_vfs_provider.h`: Wired directly into `ResourceServer` (`Vfs()`, `FormatLoaders()`, `MountProjectManifest()`) and `GameController` startup.
- [x] `engine/resource/table_schema_binder.h`: Zero-allocation `std::string_view` cell parsers with Warcraft-compatible non-zero integer boolean truthiness, wired into `CDoodadData` and `CDestructableData`.

### Stage 3 — RHI Backend Encapsulation, Dirty-Rect Overlay & Discrete Timeline Sampling
- [x] `engine/render/rhi_backend.h` & `src/render/rhi_device.h`: `IRhiBackend` implemented by `SokolRhiBackend` (GPU mode) and `NullRhiBackend` (headless CI mode), wired into `RhiDevice` and `RenderServer`.
- [x] `engine/render/standard_render_passes.h`: 100% War3-free `TerrainOverlayState` with `O(dirty_area)` incremental sub-rectangle rebaking and `DiscreteStepTimelineSampler` wired into `MdxViewer` (`TAG_KMTF` discrete slot evaluation) and `TexturedTerrain`.
- [x] `projects/war3/war3_render_adapters.h`: Houses Warcraft-specific `War3ReplaceableId` (`TeamColor`, `TeamGlow`, `Cliff`, `LordaeronTree`) and `War3MdxLayerSampler`.

### Stage 4 — RTTI-Free UI Hierarchy, Reactive MVVM HUD & Unified AudioServer
- [x] `engine/ui/ui_frame_rtti_free.h`: 15-kind inheritance bitmask (`UIFrame::As<T>()` / `DynamicCastFrame<T>()`) replacing all `dynamic_cast` across `src/ui/`.
- [x] `engine/ui/hud_view_model.h` & `src/ui/ui_data_binder.{h,cpp}`: `UIDataBinder` implements `IHudViewListener` and subscribes to `HudViewBinder`, driving `TextFrame` mutations reactively on dirty state transitions.
- [x] `src/audio/audio_server.{h,cpp}`: Unified `voiceId -> backendSoundId` tracking, load-before-submit voice validation (zero ghost `UnitVoice` ducking on missing `.wav` probes), and full backend cleanup on `StopVoice`/`StopAllVoices`/`Reset`.

### Stage 5 — RTTI-Free World Entities, Overflow-Safe Binary Streams & `"ESAV"` Snapshots
- [x] `engine/world/widget_kind.h`: O(1) `WidgetKind` tag (`CWidget::AsUnit` / `AsItem` / `AsDestructable`) replacing all `dynamic_cast` across `src/ecs/`, `src/game/`, and `src/scripting/`.
- [x] `engine/world/simulation_extension.h` & `projects/war3/war3_save_contributor.h`: Little-Endian `BinaryWriter`/`BinaryReader` with subtraction-based `len > size_ - cursor_` overflow protection and FNV-1a checksummed `"ESAV"` `DeterministicSaveArchive`, wired into `CSimulation::SaveDeterministicEsavArchive` / `LoadDeterministicEsavArchive`.

### Stage 6 — Dual-Project Plugin Host (`engine_player`) & v33 Tileset Profiles
- [x] `projects/minimal_rts/`: Standalone non-Warcraft RTS module (`minimal_rts_module` shared plugin + static test lib) with `teamId < kMaxTeams` bounds validation and `MiniJsonParser` prototype loading.
- [x] `src/game/war3/`: `War3GameModule` v33 (`war3_module_plugin` shared library) implementing `OnResolveTilesetEnvironmentProfile`, wired into `WorldSceneSession::LoadMap` and `WorldPresentationBridge` (`terrainFogHook` & `SyncAmbientSound`).
- [x] `apps/engine_player.cpp`: Zero-coupling generic host executable loading `war3_module_plugin` (`projects/war3/war3.project.json`) or `minimal_rts_module` (`projects/minimal_rts/project.json`) dynamically via C-ABI plugin hooks.
