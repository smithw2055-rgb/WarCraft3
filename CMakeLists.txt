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

if(MSVC)
    add_compile_options(/wd4819 /bigobj)
endif()

# -------------------------------------------------------------------
# Auto-detect vcpkg_installed (Windows)
# -------------------------------------------------------------------
if(PLATFORM_WINDOWS AND EXISTS "${CMAKE_SOURCE_DIR}/vcpkg_installed")
    set(VCPKG_INSTALLED_DIR "${CMAKE_SOURCE_DIR}/vcpkg_installed")
    file(GLOB VCPKG_TRIPLETS RELATIVE "${VCPKG_INSTALLED_DIR}" "${VCPKG_INSTALLED_DIR}/*")
    list(FILTER VCPKG_TRIPLETS INCLUDE REGEX "^(x64|x86|arm|arm64)-")
    list(GET VCPKG_TRIPLETS 0 VCPKG_TRIPLET)
    if(VCPKG_TRIPLET)
        message(STATUS "vcpkg_installed detected: ${VCPKG_TRIPLET}")
        list(APPEND CMAKE_PREFIX_PATH "${VCPKG_INSTALLED_DIR}/${VCPKG_TRIPLET}")
        list(APPEND CMAKE_FIND_ROOT_PATH "${VCPKG_INSTALLED_DIR}/${VCPKG_TRIPLET}")
    endif()
endif()

# -------------------------------------------------------------------
# ZLib & BZip2 - optional, needed by StormLib's config mode
find_package(ZLIB QUIET)
if(NOT ZLIB_FOUND)
    # Fallback: manual paths for vcpkg_installed
    if(DEFINED VCPKG_TRIPLET)
        find_path(ZLIB_INCLUDE_DIR zlib.h PATHS "${VCPKG_INSTALLED_DIR}/${VCPKG_TRIPLET}/include" NO_DEFAULT_PATH)
        find_library(ZLIB_LIBRARY zlib PATHS "${VCPKG_INSTALLED_DIR}/${VCPKG_TRIPLET}/lib" NO_DEFAULT_PATH)
        if(ZLIB_INCLUDE_DIR AND ZLIB_LIBRARY)
            set(ZLIB_FOUND TRUE)
            message(STATUS "ZLIB found via vcpkg_installed: ${ZLIB_LIBRARY}")
        endif()
    endif()
endif()
find_package(BZip2 QUIET)
if(NOT BZIP2_FOUND)
    if(DEFINED VCPKG_TRIPLET)
        find_path(BZIP2_INCLUDE_DIR bzlib.h PATHS "${VCPKG_INSTALLED_DIR}/${VCPKG_TRIPLET}/include" NO_DEFAULT_PATH)
        find_library(BZIP2_LIBRARY bz2 PATHS "${VCPKG_INSTALLED_DIR}/${VCPKG_TRIPLET}/lib" NO_DEFAULT_PATH)
        if(BZIP2_INCLUDE_DIR AND BZIP2_LIBRARY)
            set(BZIP2_FOUND TRUE)
            message(STATUS "BZip2 found via vcpkg_installed: ${BZIP2_LIBRARY}")
        endif()
    endif()
endif()

# -------------------------------------------------------------------
# StormLib - for W3X/W3M map file parsing
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
    message(STATUS "StormLib NOT found! Install via vcpkg (stormlib) or apt (libstorm-dev)")
endif()

# -------------------------------------------------------------------
# CascLib - for CASC storage reading (1.30+ installs)
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

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# -------------------------------------------------------------------
# Dependencies
# -------------------------------------------------------------------
if(BUILD_APP)
    find_package(OpenGL REQUIRED)
    if(PLATFORM_LINUX)
        find_package(X11 REQUIRED)
        find_library(XI_LIBRARY Xi REQUIRED)
        find_library(XCURSOR_LIBRARY Xcursor REQUIRED)
    endif()

    # NOTE: the former AeroGUI/Noesis XAML HUD was removed in favor of the
    # retail FDF UI stack (see src/ui/FdfHudHost.*). No proprietary UI SDK
    # is required to build the app.
endif()

# -------------------------------------------------------------------
# Source files
# -------------------------------------------------------------------
set(SOURCES
    src/main.cpp
    src/platform/platform.cpp
    src/render/renderer.cpp
    src/render/shader.cpp
    src/render/camera.cpp
    src/render/terrain.cpp
    src/render/textured_terrain.cpp
    src/render/shapes.cpp
    src/render/terrain_shader.cpp
    src/render/shaders_common.cpp
    src/render/shaders_common.h
    src/render/shadow_manager.cpp
    src/render/fog_of_war.cpp
    src/render/splat_model.cpp
    src/render/software_mesh.cpp
    src/render/wave_builder.cpp
    src/render/minimap.cpp
    src/render/daynight.cpp
    src/resource/mpq_storm.cpp
        src/resource/casc.cpp
    src/resource/slk.cpp
    src/asset/stb_image.cpp
    src/asset/stb_image_write.cpp
    src/asset/Texture.cpp
    src/asset/BLP.cpp
    src/asset/DDS.cpp
    src/asset/TileSet.cpp
    src/map/w3e.cpp
    src/map/w3x_storm.cpp
    src/map/w3i.cpp
    src/map/wpm.cpp
    src/map/w3r.cpp
    src/map/wts.cpp
    src/map/object_mods.cpp
    src/map/w3d.cpp
    src/map/unitsdoo.cpp
    src/model/mdx.cpp
    src/model/mdl.cpp
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
    src/render/light_manager.cpp
    src/render/scene.cpp
    src/render/replaceable_ids.cpp
    src/sokol_impl.cpp
    # ===== New utility modules =====
    src/core/path_utils.cpp
    src/render/picking.cpp
    src/render/unit_selection_geom.cpp
    src/render/splat_metadata.cpp
    src/ecs/unit_text_utils.cpp
    src/ecs/animation_utils.cpp
    # ===== New ECS / Data Layer =====
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
    src/ecs/unit.cpp
    src/ecs/fog_state.cpp
    src/ecs/player.cpp
    src/ecs/destructable.cpp
    src/ecs/item.cpp
    src/ecs/csimulation.cpp
    src/ecs/lockstep.cpp
    src/net/tcp_transport.cpp
    src/net/lobby_state.cpp
    src/net/turn_batch.cpp
    src/net/turn_manager.cpp
    src/net/lockstep_host.cpp
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
    src/ecs/abilities/spells.cpp
    src/ecs/abilities/spells2.cpp
    src/ecs/abilities/spells3.cpp
    src/ecs/abilities/spells4.cpp
    src/ecs/abilities/spells5.cpp
    src/ecs/abilities/spells6.cpp
    src/ecs/abilities/spells7.cpp
    src/ecs/abilities/spells8.cpp
    src/ecs/abilities/spells9.cpp
    src/ecs/abilities/spells10.cpp
    src/ecs/abilities/spells11.cpp
    src/ecs/abilities/spells12.cpp
    src/ecs/abilities/spells13.cpp
    src/ecs/abilities/spells14.cpp
    src/ecs/abilities/spells15.cpp
    src/ecs/abilities/spells16.cpp
    src/ecs/abilities/spells17.cpp
    src/ecs/abilities/spells18.cpp
    src/ecs/abilities/spells19.cpp
    src/ecs/abilities/spells20.cpp
    src/ecs/abilities/spells21.cpp
    src/ecs/abilities/spells22.cpp
    src/ecs/abilities/spells23.cpp
    src/ecs/abilities/spells24.cpp
    src/ecs/abilities/spells25.cpp
    src/ecs/abilities/spells26.cpp
    src/ecs/abilities/spells27.cpp
    src/ecs/abilities/spells28.cpp
    src/ecs/abilities/spells29.cpp
    src/ecs/abilities/ability_repair.cpp
    src/ecs/abilities/ability_transport.cpp
    src/ecs/abilities/ability_root.cpp
    src/ecs/abilities/ability_aura.cpp
    src/ecs/abilities/auras.cpp
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
    src/ecs/combat/combat_system.cpp
    src/ecs/combat/projectile.cpp
    # ===== Scripting =====
    src/scripting/JassLexer.cpp
    src/scripting/JassParser.cpp
    src/scripting/JassVM.cpp
    src/scripting/jass_animation_tags.cpp
    src/scripting/JassNatives.cpp
    # ===== Audio =====
    src/audio/AudioSystem.cpp
    src/audio/UnitSoundset.cpp
    src/audio/AckSoundBank.cpp
    # ===== UI (retail FDF stack; no proprietary SDK) =====
    src/ui/UiRenderer.cpp
    src/ui/ConsoleHUD.cpp
    src/ui/FdfHudHost.cpp
    src/ui/FrameDefinition.cpp
    src/ui/FdfParser.cpp
    src/ui/UIFrame.cpp
    src/ui/SimpleFrame.cpp
    src/ui/TextureFrame.cpp
    src/ui/BackdropFrame.cpp
    src/ui/CommandCard.cpp
    src/ui/MainUI.cpp
    src/ui/TextRenderer.cpp
    src/ui/TextFrame.cpp
    src/ui/InfoPanelFrame.cpp
    src/ui/PortraitFrame.cpp
    src/ui/ResourceBarFrame.cpp
    src/ui/ButtonFrame.cpp
    src/ui/TooltipFrame.cpp
    src/ui/HeroPanelFrame.cpp
    src/ui/ChatConsoleFrame.cpp
    src/ui/FrameBuilder.cpp
    src/ui/MultiSelectionPanel.cpp
    src/ui/BuffBar.cpp
    # ===== Core / State Machine =====
    src/core/game_controller.cpp
    src/core/map_path_resolver.cpp
    src/core/warsmash_config.cpp
    # ===== Map Loading =====
    src/map/map_loader.cpp
    # ===== JASS Environments =====
    src/scripting/jass_config_env.cpp
    src/scripting/jass_common_env.cpp
)

if(BUILD_APP)
    # Add executable
    add_executable(warsmash ${SOURCES})

    target_include_directories(warsmash PRIVATE src)

    # Pass project source root to code so it can find assets regardless of build location
    target_compile_definitions(warsmash PRIVATE
        PROJECT_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
        __STORMLIB_NO_STATIC_LINK__
    )

    # -- Link StormLib (optional - app works without MPQ access) --
    macro(link_stormlib target)
        if(StormLib_FOUND)
            target_link_libraries(${target} PRIVATE StormLib::storm)
            # Copy runtime DLLs to output directory (Windows)
            if(PLATFORM_WINDOWS AND DEFINED VCPKG_TRIPLET)
                set(VCPKG_BIN "${VCPKG_INSTALLED_DIR}/${VCPKG_TRIPLET}/bin")
                foreach(DLL_FILE stormlib.dll zlib1.dll bz2.dll)
                    set(DLL_PATH "${VCPKG_BIN}/${DLL_FILE}")
                    if(EXISTS "${DLL_PATH}")
                        add_custom_command(TARGET ${target} POST_BUILD
                            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                                "${DLL_PATH}"
                                "$<TARGET_FILE_DIR:${target}>/"
                            COMMENT "Copying ${DLL_FILE} to output"
                        )
                    endif()
                endforeach()
            endif()
        else()
            message(WARNING "Building without StormLib - MPQ/terrain loading disabled")
            target_compile_definitions(${target} PRIVATE WITHOUT_STORMLIB=1)
        endif()
    endmacro()
    macro(link_casclib target)
        if(CascLib_FOUND)
            target_link_libraries(${target} PRIVATE CascLib::casc)
        else()
            message(WARNING "Building without CascLib - CASC storage disabled")
            target_compile_definitions(${target} PRIVATE WITHOUT_CASCLIB=1)
        endif()
    endmacro()

    # -- Linking --
    # sokol headers are header-only; sokol_app.h on MSVC uses #pragma comment for system libs
    target_compile_definitions(warsmash PRIVATE SOKOL_GLCORE)
    if(PLATFORM_WINDOWS)
        target_link_libraries(warsmash PRIVATE
            ${OPENGL_LIBRARIES}
        )
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

    # ---- Resource Auditor tool (console-only) ----
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

    add_executable(dump_cliff
        tools/dump_cliff.cpp
        src/resource/mpq_storm.cpp
        src/resource/casc.cpp
        src/resource/slk.cpp
    )
    target_include_directories(dump_cliff PRIVATE src)
    target_compile_definitions(dump_cliff PRIVATE
        PROJECT_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
        __STORMLIB_NO_STATIC_LINK__
    )
    link_stormlib(dump_cliff)
    link_casclib(dump_cliff)

    add_executable(search_mdx
        tools/search_mdx.cpp
        src/resource/mpq_storm.cpp
        src/resource/casc.cpp
    )
    target_include_directories(search_mdx PRIVATE src)
    target_compile_definitions(search_mdx PRIVATE
        PROJECT_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
        __STORMLIB_NO_STATIC_LINK__
    )
    link_stormlib(search_mdx)
    link_casclib(search_mdx)

    add_executable(mpq_file_probe
        tools/mpq_file_probe.cpp
        src/resource/mpq_storm.cpp
        src/resource/casc.cpp
    )
    target_include_directories(mpq_file_probe PRIVATE src)
    target_compile_definitions(mpq_file_probe PRIVATE
        PROJECT_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
        __STORMLIB_NO_STATIC_LINK__
    )
    link_stormlib(mpq_file_probe)
    link_casclib(mpq_file_probe)

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
    target_include_directories(extract_ui_assets PRIVATE src)
    target_compile_definitions(extract_ui_assets PRIVATE
        PROJECT_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
        __STORMLIB_NO_STATIC_LINK__
    )
    link_stormlib(extract_ui_assets)
    link_casclib(extract_ui_assets)

    target_include_directories(resource_auditor PRIVATE src)
    target_compile_definitions(resource_auditor PRIVATE
        PROJECT_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
        __STORMLIB_NO_STATIC_LINK__
    )
    link_stormlib(resource_auditor)
    link_casclib(resource_auditor)

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
    target_include_directories(terrain_analyzer PRIVATE src)
    target_compile_definitions(terrain_analyzer PRIVATE
        PROJECT_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
        __STORMLIB_NO_STATIC_LINK__
    )
    link_stormlib(terrain_analyzer)
    link_casclib(terrain_analyzer)
endif()
