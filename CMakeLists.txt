cmake_minimum_required(VERSION 3.15)
project(WarsmashCpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
option(BUILD_TESTS "Build unit tests" ON)
option(BUILD_APP "Build warsmash executable" ON)

# -------------------------------------------------------------------
# Platform detection
# -------------------------------------------------------------------
if(WIN32)
    set(PLATFORM_WINDOWS TRUE)
    message(STATUS "Target: Windows")
else()
    set(PLATFORM_LINUX TRUE)
    message(STATUS "Target: Linux / WSL")
endif()

if(PLATFORM_WINDOWS)
    add_compile_definitions(NOMINMAX)
endif()

if(MSVC)
    string(REGEX REPLACE "/GR( |$)" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    foreach(flag_var CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELWITHDEBINFO)
        if(DEFINED ${flag_var})
            string(REPLACE "/Zi" "/Z7" ${flag_var} "${${flag_var}}")
        endif()
    endforeach()
    add_compile_options(/wd4819 /bigobj)
endif()

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)

# -------------------------------------------------------------------
# Auto-detect vcpkg_installed (Windows)
# -------------------------------------------------------------------
if(PLATFORM_WINDOWS AND EXISTS "${CMAKE_SOURCE_DIR}/vcpkg_installed")
    set(VCPKG_INSTALLED_DIR "${CMAKE_SOURCE_DIR}/vcpkg_installed")
    file(GLOB VCPKG_TRIPLETS RELATIVE "${VCPKG_INSTALLED_DIR}" "${VCPKG_INSTALLED_DIR}/*")
    list(FILTER VCPKG_TRIPLETS INCLUDE REGEX "^(x64|x86|arm|arm64)-")
    if(VCPKG_TRIPLETS)
        list(GET VCPKG_TRIPLETS 0 VCPKG_TRIPLET)
    endif()
    if(VCPKG_TRIPLET)
        message(STATUS "vcpkg_installed detected: ${VCPKG_TRIPLET}")
        list(APPEND CMAKE_PREFIX_PATH "${VCPKG_INSTALLED_DIR}/${VCPKG_TRIPLET}")
        list(APPEND CMAKE_FIND_ROOT_PATH "${VCPKG_INSTALLED_DIR}/${VCPKG_TRIPLET}")
    endif()
endif()

# -------------------------------------------------------------------
# ZLib & BZip2 - optional, used by StormLib & NativeMpqArchive fallback
# -------------------------------------------------------------------
find_package(ZLIB QUIET)
if(NOT ZLIB_FOUND)
    # Fallback: manual paths for vcpkg_installed
    if(DEFINED VCPKG_TRIPLET)
        find_path(ZLIB_INCLUDE_DIR zlib.h PATHS "${VCPKG_INSTALLED_DIR}/${VCPKG_TRIPLET}/include" NO_DEFAULT_PATH)
        find_library(ZLIB_LIBRARY NAMES zlib zlibd z PATHS "${VCPKG_INSTALLED_DIR}/${VCPKG_TRIPLET}/lib" NO_DEFAULT_PATH)
        if(ZLIB_INCLUDE_DIR AND ZLIB_LIBRARY)
            set(ZLIB_FOUND TRUE)
            set(ZLIB_INCLUDE_DIRS "${ZLIB_INCLUDE_DIR}")
            set(ZLIB_LIBRARIES "${ZLIB_LIBRARY}")
            message(STATUS "ZLIB found via vcpkg_installed: ${ZLIB_LIBRARY}")
        endif()
    endif()
endif()

find_package(BZip2 QUIET)
if(NOT BZIP2_FOUND)
    if(DEFINED VCPKG_TRIPLET)
        find_path(BZIP2_INCLUDE_DIR bzlib.h PATHS "${VCPKG_INSTALLED_DIR}/${VCPKG_TRIPLET}/include" NO_DEFAULT_PATH)
        find_library(BZIP2_LIBRARY NAMES bz2 bz2d PATHS "${VCPKG_INSTALLED_DIR}/${VCPKG_TRIPLET}/lib" NO_DEFAULT_PATH)
        if(BZIP2_INCLUDE_DIR AND BZIP2_LIBRARY)
            set(BZIP2_FOUND TRUE)
            set(BZIP2_LIBRARIES "${BZIP2_LIBRARY}")
            message(STATUS "BZip2 found via vcpkg_installed: ${BZIP2_LIBRARY}")
        endif()
    endif()
endif()

# -------------------------------------------------------------------
# StormLib - for W3X/W3M/MPQ archive parsing
# -------------------------------------------------------------------
find_package(StormLib CONFIG QUIET)
if(NOT StormLib_FOUND)
    # Fallback: manual find (vcpkg or custom install)
    if(DEFINED VCPKG_TRIPLET)
        find_package(StormLib CONFIG QUIET PATHS "${VCPKG_INSTALLED_DIR}/${VCPKG_TRIPLET}/share")
    endif()
endif()
if(NOT StormLib_FOUND)
    find_path(STORMLIB_INCLUDE_DIR StormLib.h)
    find_library(STORMLIB_LIBRARY NAMES storm stormlib stormlib_static StormLib)
    if(STORMLIB_INCLUDE_DIR AND STORMLIB_LIBRARY)
        add_library(StormLib::storm UNKNOWN IMPORTED)
        set_target_properties(StormLib::storm PROPERTIES
            IMPORTED_LOCATION "${STORMLIB_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${STORMLIB_INCLUDE_DIR}"
            INTERFACE_COMPILE_DEFINITIONS "__STORMLIB_NO_STATIC_LINK__"
        )
        set(StormLib_FOUND TRUE)
        message(STATUS "StormLib found via fallback: ${STORMLIB_LIBRARY}")
    endif()
endif()

if(StormLib_FOUND)
    message(STATUS "StormLib found")
else()
    message(STATUS "StormLib NOT found - using streaming NativeMpqArchive fallback")
endif()

# -------------------------------------------------------------------
# CascLib - for CASC storage reading (1.30+ installs)
# -------------------------------------------------------------------
find_path(CASCLIB_INCLUDE_DIR CascLib.h
    PATHS "${VCPKG_INSTALLED_DIR}/${VCPKG_TRIPLET}/include" /usr/local/include /usr/include)
find_library(CASCLIB_LIBRARY NAMES casc CascLib casclib
    PATHS "${VCPKG_INSTALLED_DIR}/${VCPKG_TRIPLET}/lib" /usr/local/lib /usr/lib)
if(CASCLIB_INCLUDE_DIR AND CASCLIB_LIBRARY)
    add_library(CascLib::casc UNKNOWN IMPORTED)
    set_target_properties(CascLib::casc PROPERTIES
        IMPORTED_LOCATION "${CASCLIB_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${CASCLIB_INCLUDE_DIR}"
    )
    set(CascLib_FOUND TRUE)
    message(STATUS "CascLib found: ${CASCLIB_LIBRARY}")
else()
    message(STATUS "CascLib NOT found! Install via vcpkg (casclib) or apt (libcasc-dev)")
endif()

# -------------------------------------------------------------------
# Cross-platform helper macros (Zero-RTTI, StormLib/CascLib/ZLIB/BZip2, DLL copy)
# -------------------------------------------------------------------
macro(enable_zero_rtti target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /GR-)
    else()
        target_compile_options(${target} PRIVATE -fno-rtti)
    endif()
endmacro()

macro(copy_vcpkg_runtime_dlls target)
    if(PLATFORM_WINDOWS AND DEFINED VCPKG_TRIPLET)
        set(VCPKG_BIN "${VCPKG_INSTALLED_DIR}/${VCPKG_TRIPLET}/bin")
        foreach(DLL_FILE stormlib.dll zlib1.dll bz2.dll)
            set(DLL_PATH "${VCPKG_BIN}/${DLL_FILE}")
            if(EXISTS "${DLL_PATH}")
                add_custom_command(TARGET ${target} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${DLL_PATH}"
                        "$<TARGET_FILE_DIR:${target}>/"
                    COMMENT "Copying ${DLL_FILE} to $<TARGET_FILE_DIR:${target}>"
                )
            endif()
        endforeach()
    endif()
endmacro()

macro(link_compression_libs target)
    if(ZLIB_FOUND)
        target_compile_definitions(${target} PRIVATE HAVE_ZLIB=1)
        if(ZLIB_INCLUDE_DIRS)
            target_include_directories(${target} PRIVATE ${ZLIB_INCLUDE_DIRS})
        elseif(ZLIB_INCLUDE_DIR)
            target_include_directories(${target} PRIVATE ${ZLIB_INCLUDE_DIR})
        endif()
        if(TARGET ZLIB::ZLIB)
            target_link_libraries(${target} PRIVATE ZLIB::ZLIB)
        elseif(ZLIB_LIBRARIES)
            target_link_libraries(${target} PRIVATE ${ZLIB_LIBRARIES})
        elseif(ZLIB_LIBRARY)
            target_link_libraries(${target} PRIVATE ${ZLIB_LIBRARY})
        endif()
    endif()
    if(BZIP2_FOUND)
        target_compile_definitions(${target} PRIVATE HAVE_BZIP2=1)
        if(BZIP2_INCLUDE_DIR)
            target_include_directories(${target} PRIVATE ${BZIP2_INCLUDE_DIR})
        endif()
        if(TARGET BZip2::BZip2)
            target_link_libraries(${target} PRIVATE BZip2::BZip2)
        elseif(BZIP2_LIBRARIES)
            target_link_libraries(${target} PRIVATE ${BZIP2_LIBRARIES})
        elseif(BZIP2_LIBRARY)
            target_link_libraries(${target} PRIVATE ${BZIP2_LIBRARY})
        endif()
    endif()
endmacro()

macro(link_stormlib target)
    if(StormLib_FOUND)
        target_link_libraries(${target} PRIVATE StormLib::storm)
    else()
        target_compile_definitions(${target} PRIVATE WITHOUT_STORMLIB=1)
    endif()
    link_compression_libs(${target})
    copy_vcpkg_runtime_dlls(${target})
endmacro()

macro(link_casclib target)
    if(CascLib_FOUND)
        target_link_libraries(${target} PRIVATE CascLib::casc)
    else()
        target_compile_definitions(${target} PRIVATE WITHOUT_CASCLIB=1)
    endif()
endmacro()

# -------------------------------------------------------------------
# Stage 1-6 Engine Foundation & Project Plugin Targets
# -------------------------------------------------------------------
add_library(engine_foundation INTERFACE)
target_include_directories(engine_foundation INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)
target_compile_options(engine_foundation INTERFACE
    $<IF:$<CXX_COMPILER_ID:MSVC>,/GR-,-fno-rtti>
)

# Static MinimalRTS library (for fast in-process unit test verification)
add_library(minimal_rts_module_static STATIC
    projects/minimal_rts/minimal_rts_module.cpp
)
target_include_directories(minimal_rts_module_static PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(minimal_rts_module_static PRIVATE engine_foundation)
enable_zero_rtti(minimal_rts_module_static)

# Shared MinimalRTS plugin (.dll / .so dynamically loaded by engine_player)
add_library(minimal_rts_module SHARED
    projects/minimal_rts/minimal_rts_module.cpp
)
target_compile_definitions(minimal_rts_module PRIVATE MINIMAL_RTS_SHARED_PLUGIN=1)
target_link_libraries(minimal_rts_module PRIVATE engine_foundation)
set_target_properties(minimal_rts_module PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)
enable_zero_rtti(minimal_rts_module)

# Zero-coupling generic engine player host (links ONLY engine_foundation + OS dlopen)
add_executable(engine_player
    apps/engine_player.cpp
)
target_link_libraries(engine_player PRIVATE engine_foundation)
if(PLATFORM_LINUX)
    target_link_libraries(engine_player PRIVATE dl pthread m)
endif()
enable_zero_rtti(engine_player)

# Unified VFS / MPQ asset inspection CLI
add_executable(engine_asset_cli
    apps/engine_asset_cli.cpp
    src/resource/mpq_storm.cpp
    src/resource/casc.cpp
)
target_include_directories(engine_asset_cli PRIVATE src ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/third_party)
target_compile_definitions(engine_asset_cli PRIVATE
    PROJECT_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
    __STORMLIB_NO_STATIC_LINK__
)
link_stormlib(engine_asset_cli)
link_casclib(engine_asset_cli)
enable_zero_rtti(engine_asset_cli)

# -------------------------------------------------------------------
# Shared WarCraft III Core/Simulation/Plugin Sources
# -------------------------------------------------------------------
set(WAR3_CORE_SOURCES
    src/render/render_server.cpp
    src/render/render_graph.cpp
    src/render/texture_atlas_packer.cpp
    src/render/camera.cpp
    src/render/fog_of_war.cpp
    src/render/mdx_viewer.cpp
    src/render/mdx_sequence_select.cpp
    src/render/mdx_animator.cpp
    src/render/mdx_collision.cpp
    src/render/mdx_event_effects.cpp
    src/render/mdx_event_splat.cpp
    src/render/mdx_skinning.cpp
    src/render/mdx_particles.cpp
    src/render/mdx_prem_particles.cpp
    src/render/mdx_ribbons.cpp
    src/render/lightning.cpp
    src/render/scene.cpp
    src/render/replaceable_ids.cpp
    src/render/picking.cpp
    src/resource/mpq_storm.cpp
    src/resource/casc.cpp
    src/resource/slk.cpp
    src/resource/resource_server.cpp
    src/audio/audio_server.cpp
    src/audio/UnitSoundset.cpp
    src/audio/AckSoundBank.cpp
    src/audio/KeyedSounds.cpp
    src/audio/MidiFile.cpp
    src/audio/MidiSynth.cpp
    src/asset/stb_image.cpp
    src/asset/stb_image_write.cpp
    src/asset/Texture.cpp
    src/asset/BLP.cpp
    src/asset/DDS.cpp
    src/asset/MipChain.cpp
    src/map/w3e.cpp
    src/map/w3x_storm.cpp
    src/map/w3i.cpp
    src/map/wpm.cpp
    src/map/w3r.cpp
    src/map/wts.cpp
    src/map/object_mods.cpp
    src/map/w3d.cpp
    src/map/unitsdoo.cpp
    src/map/path_tex.cpp
    src/model/mdx.cpp
    src/model/mdl.cpp
    src/core/path_utils.cpp
    src/core/job_system.cpp
    src/core/map_path_resolver.cpp
    src/core/world_scene_session.cpp
    src/core/input_action_map.cpp
    src/core/engine_profiler.cpp
    src/ecs/unit_text_utils.cpp
    src/ecs/animation_state.cpp
    src/data/gameplay_constants.cpp
    src/data/combat_types.cpp
    src/data/unitdata.cpp
    src/data/abilitydata.cpp
    src/data/itemdata.cpp
    src/data/destructabledata.cpp
    src/data/doodaddata.cpp
    src/data/slk_merge.cpp
    src/data/upgradedata.cpp
    src/ecs/widget.cpp
    src/ecs/unit_type.cpp
    src/ecs/attribute_aggregator.cpp
    src/ecs/unit.cpp
    src/ecs/fog_state.cpp
    src/ecs/player.cpp
    src/ecs/destructable.cpp
    src/ecs/item.cpp
    src/ecs/csimulation.cpp
    src/ecs/sim_snapshot.cpp
    src/ecs/hero_carry.cpp
    src/ecs/lockstep.cpp
    src/net/tcp_transport.cpp
    src/net/lobby_state.cpp
    src/net/turn_batch.cpp
    src/net/turn_manager.cpp
    src/net/lockstep_host.cpp
    src/net/snapshot_resync.cpp
    src/ecs/melee_ai.cpp
    src/ecs/ability.cpp
    src/ecs/abilities/ability_attack.cpp
    src/ecs/abilities/ability_move.cpp
    src/ecs/abilities/ability_hero.cpp
    src/ecs/abilities/ability_inventory.cpp
    src/ecs/abilities/ability_build.cpp
    src/ecs/abilities/ability_builder.cpp
    src/ecs/abilities/ability_harvest.cpp
    src/ecs/abilities/ability_gold_mine.cpp
    src/ecs/abilities/ability_return_resources.cpp
    src/ecs/abilities/ability_queue.cpp
    src/ecs/abilities/ability_rally.cpp
    src/ecs/abilities/ability_revive_hero.cpp
    src/ecs/abilities/ability_upgrade.cpp
    src/ecs/abilities/ability_sell_items.cpp
    src/ecs/abilities/ability_spell_base.cpp
    src/ecs/abilities/ability_repair.cpp
    src/ecs/abilities/ability_transport.cpp
    src/ecs/abilities/ability_root.cpp
    src/ecs/abilities/ability_aura.cpp
    src/ecs/ability_factory.cpp
    src/ecs/tech_tree.cpp
    src/ecs/upgrade_effect.cpp
    src/ecs/item_bonuses.cpp
    src/ecs/buff.cpp
    src/ecs/buffs.cpp
    src/ecs/orders/order.cpp
    src/ecs/behaviors/behavior_move.cpp
    src/ecs/behaviors/behavior_attack.cpp
    src/ecs/behaviors/behavior_stop.cpp
    src/ecs/behaviors/behavior_hold_position.cpp
    src/ecs/behaviors/behavior_follow.cpp
    src/ecs/behaviors/behavior_patrol.cpp
    src/ecs/behaviors/behavior_attack_move.cpp
    src/ecs/behaviors/behavior_stun.cpp
    src/ecs/behaviors/behavior_board_transport.cpp
    src/ecs/behaviors/behavior_build.cpp
    src/ecs/behaviors/behavior_repair.cpp
    src/ecs/behaviors/behavior_harvest.cpp
    src/ecs/behaviors/behavior_return_resources.cpp
    src/ecs/behaviors/behavior_spell.cpp
    src/ecs/behaviors/behavior_root.cpp
    src/ecs/behaviors/behavior_channel.cpp
    src/ecs/behaviors/behavior_item.cpp
    src/ecs/pathing/world_collision.cpp
    src/ecs/pathing/pathfinding.cpp
    src/ecs/pathing/pathfinding_server.cpp
    src/ecs/pathing/crowd_steering.cpp
    src/ecs/combat/combat_system.cpp
    src/ecs/combat/projectile.cpp
    src/engine/engine_kernel.cpp
    src/engine/dynamic_module_loader.cpp
    src/game/war3/war3_game_module.cpp
    src/game/war3/abilities/war3_spells.cpp
    src/game/war3/war3_module_plugin_export.cpp
    src/scripting/JassLexer.cpp
    src/scripting/JassParser.cpp
    src/scripting/JassVM.cpp
    src/scripting/script_engine_host.cpp
    src/scripting/JassBytecode.cpp
    src/scripting/jass_animation_tags.cpp
    src/scripting/JassNatives.cpp
    src/scripting/natives/jass_natives_units.cpp
    src/scripting/natives/jass_natives_triggers.cpp
    src/scripting/natives/jass_natives_environment.cpp
    src/scripting/natives/jass_natives_ui_data.cpp
    src/scripting/jass_config_env.cpp
    src/scripting/jass_common_env.cpp
    src/ui/UiRenderer.cpp
    src/ui/FdfParser.cpp
    src/ui/FrameDefinition.cpp
    src/ui/UIFrame.cpp
    src/ui/TextureFrame.cpp
    src/ui/BackdropFrame.cpp
    src/ui/TextRenderer.cpp
    src/ui/TextFrame.cpp
    src/ui/ButtonFrame.cpp
    src/ui/FrameBuilder.cpp
    src/ui/ui_data_binder.cpp
)

# Dynamically loadable WarCraft III plugin (`libwar3_module_plugin.so` / `war3_module_plugin.dll`)
# Used by both `engine_player` (`projects/war3/war3.project.json`) and `DynamicModuleLoader`!
add_library(war3_module_plugin SHARED
    ${WAR3_CORE_SOURCES}
    tests/test_render_stubs.cpp
)
target_include_directories(war3_module_plugin PRIVATE src ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/third_party)
target_compile_definitions(war3_module_plugin PRIVATE
    PROJECT_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
    __STORMLIB_NO_STATIC_LINK__
)
link_stormlib(war3_module_plugin)
link_casclib(war3_module_plugin)
if(PLATFORM_LINUX)
    target_link_options(war3_module_plugin PRIVATE -Wl,--no-undefined)
    target_link_options(minimal_rts_module PRIVATE -Wl,--no-undefined)
    target_link_libraries(war3_module_plugin PRIVATE dl pthread m)
endif()
set_target_properties(war3_module_plugin PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)
enable_zero_rtti(war3_module_plugin)

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# -------------------------------------------------------------------
# Windowed Application & Offline Asset Analysis Tools
# -------------------------------------------------------------------
if(BUILD_APP)
    find_package(OpenGL REQUIRED)
    if(PLATFORM_LINUX)
        find_package(X11 REQUIRED)
        find_library(XI_LIBRARY Xi REQUIRED)
        find_library(XCURSOR_LIBRARY Xcursor REQUIRED)
    endif()

    set(APP_ONLY_SOURCES
        src/main.cpp
        src/platform/platform.cpp
        src/render/textured_terrain.cpp
        src/render/shapes.cpp
        src/render/terrain_shader.cpp
        src/render/shaders_common.cpp
        src/render/shaders_common.h
        src/render/splat_model.cpp
        src/render/wave_builder.cpp
        src/render/minimap.cpp
        src/render/daynight.cpp
        src/render/light_manager.cpp
        src/render/unit_selection_geom.cpp
        src/render/splat_metadata.cpp
        src/asset/TileSet.cpp
        src/sokol_impl.cpp
        src/ui/ConsoleHUD.cpp
        src/ui/FdfHudHost.cpp
        src/ui/MainUI.cpp
        src/core/game_controller.cpp
        src/ecs/animation_utils.cpp
        src/core/world_presentation_bridge.cpp
        src/core/input_selection_controller.cpp
        src/core/game_bootstrap_diagnostics.cpp
        src/core/warsmash_config.cpp
        src/map/map_loader.cpp
    )

    add_executable(warsmash ${WAR3_CORE_SOURCES} ${APP_ONLY_SOURCES})
    target_include_directories(warsmash PRIVATE src ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/third_party)
    target_compile_definitions(warsmash PRIVATE
        PROJECT_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
        __STORMLIB_NO_STATIC_LINK__
        SOKOL_GLCORE
    )
    enable_zero_rtti(warsmash)

    if(PLATFORM_WINDOWS)
        target_link_libraries(warsmash PRIVATE ${OPENGL_LIBRARIES})
        link_stormlib(warsmash)
        link_casclib(warsmash)
        set_target_properties(warsmash PROPERTIES WIN32_EXECUTABLE FALSE)
    else()
        target_include_directories(warsmash PRIVATE ${OPENGL_INCLUDE_DIRS} ${X11_INCLUDE_DIR})
        target_link_libraries(warsmash PRIVATE
            ${OPENGL_LIBRARIES}
            ${X11_LIBRARIES}
            ${XI_LIBRARY}
            ${XCURSOR_LIBRARY}
            dl pthread m
        )
        link_stormlib(warsmash)
        link_casclib(warsmash)
    endif()

    # ---- Retained Offline Asset Analysis Tools (zero orphan .cpp files!) ----
    add_executable(resource_auditor
        tools/resource_auditor.cpp
        src/resource/mpq_storm.cpp
        src/resource/casc.cpp
        src/resource/slk.cpp
        src/map/w3e.cpp
        src/map/w3x_storm.cpp
        src/asset/BLP.cpp
        src/asset/stb_image.cpp
    )
    target_include_directories(resource_auditor PRIVATE src ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/third_party)
    target_compile_definitions(resource_auditor PRIVATE
        PROJECT_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
        __STORMLIB_NO_STATIC_LINK__
    )
    link_stormlib(resource_auditor)
    link_casclib(resource_auditor)
    enable_zero_rtti(resource_auditor)

    add_executable(extract_ui_assets
        tools/extract_ui_assets.cpp
        src/resource/mpq_storm.cpp
        src/resource/casc.cpp
        src/resource/slk.cpp
        src/asset/BLP.cpp
        src/model/mdx.cpp
        src/model/mdl.cpp
        src/asset/stb_image.cpp
        src/asset/stb_image_write.cpp
    )
    target_include_directories(extract_ui_assets PRIVATE src ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/third_party)
    target_compile_definitions(extract_ui_assets PRIVATE
        PROJECT_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
        __STORMLIB_NO_STATIC_LINK__
    )
    link_stormlib(extract_ui_assets)
    link_casclib(extract_ui_assets)
    enable_zero_rtti(extract_ui_assets)

    add_executable(terrain_analyzer
        tools/terrain_analyzer.cpp
        src/resource/mpq_storm.cpp
        src/resource/casc.cpp
        src/resource/slk.cpp
        src/map/w3e.cpp
        src/map/w3x_storm.cpp
        src/asset/BLP.cpp
        src/asset/stb_image.cpp
    )
    target_include_directories(terrain_analyzer PRIVATE src ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/third_party)
    target_compile_definitions(terrain_analyzer PRIVATE
        PROJECT_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
        __STORMLIB_NO_STATIC_LINK__
    )
    link_stormlib(terrain_analyzer)
    link_casclib(terrain_analyzer)
    enable_zero_rtti(terrain_analyzer)
endif()
