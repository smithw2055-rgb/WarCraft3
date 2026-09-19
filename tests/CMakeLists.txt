cmake_minimum_required(VERSION 3.15)

if(StormLib_FOUND)
    set(TEST_STORM_SOURCES test_mpq_storm.cpp)
endif()

add_executable(warsmash_tests
    test_main.cpp
    test_w3e.cpp
    test_blp.cpp
    test_mdx.cpp
    test_jass_animation_tags.cpp
    test_jass_vm.cpp
    test_render_stubs.cpp
    test_step1_bonuses.cpp
    test_aura_fixes.cpp
    test_region_events.cpp
    test_destructable_enum.cpp
    test_combat_aoe.cpp
    test_object_mods.cpp
    test_aura_combat.cpp
    test_root_build.cpp
    test_sync_sleep.cpp
    test_combat_pipeline.cpp
    test_maxlife_layers.cpp
    test_gameplay_data.cpp
    test_projectiles.cpp
    test_transport.cpp
    test_ability_builder.cpp
    test_vfs_overlay.cpp
    test_map_data.cpp
    test_jass_orders_terrain.cpp
    test_jass_visuals.cpp
    test_jass_pools.cpp
    test_jass_unitstate.cpp
    test_jass_gameflow.cpp
    test_jass_uistate.cpp
    test_jass_boards.cpp
    test_jass_terrainedit.cpp
    test_jass_struct.cpp
    test_melee_ai.cpp
    test_ribbons.cpp
    test_jass_unimplemented.cpp
    ${TEST_STORM_SOURCES}
    ../src/map/w3e.cpp
    ../src/map/w3i.cpp
    ../src/map/object_mods.cpp
    ../src/map/unitsdoo.cpp
    ../src/map/wts.cpp
    ../src/asset/BLP.cpp
    ../src/asset/DDS.cpp
    ../src/asset/stb_image.cpp
    ../src/core/map_path_resolver.cpp
    ../src/model/mdx.cpp
    ../src/render/mdx_animator.cpp
    ../src/render/mdx_collision.cpp
    ../src/render/picking.cpp
    ../src/render/mdx_event_effects.cpp
    ../src/render/mdx_event_splat.cpp
    ../src/render/mdx_sequence_select.cpp
    ../src/render/mdx_skinning.cpp
    ../src/render/mdx_prem_particles.cpp
    ../src/render/mdx_ribbons.cpp
    ../src/render/replaceable_ids.cpp
    ../src/scripting/jass_animation_tags.cpp
    ../src/scripting/JassNatives.cpp
    ../src/scripting/JassLexer.cpp
    ../src/scripting/JassParser.cpp
    ../src/scripting/JassVM.cpp
    ../src/resource/mpq_storm.cpp
    ../src/resource/slk.cpp
    ../src/map/wpm.cpp
    ../src/map/wts.cpp
    ../src/map/object_mods.cpp
    ../src/audio/AudioSystem.cpp
    ../src/data/combat_types.cpp
    ../src/data/gameplay_constants.cpp
    ../src/data/unitdata.cpp
    ../src/data/abilitydata.cpp
    ../src/data/itemdata.cpp
    ../src/data/destructabledata.cpp
    ../src/data/upgradedata.cpp
    ../src/ecs/widget.cpp
    ../src/ecs/unit.cpp
    ../src/ecs/unit_type.cpp
    ../src/ecs/player.cpp
    ../src/ecs/csimulation.cpp
    ../src/ecs/melee_ai.cpp
    ../src/ecs/ability.cpp
    ../src/ecs/ability_factory.cpp
    ../src/ecs/buff.cpp
    ../src/ecs/buffs.cpp
    ../src/ecs/item.cpp
    ../src/ecs/item_bonuses.cpp
    ../src/ecs/destructable.cpp
    ../src/ecs/tech_tree.cpp
    ../src/ecs/upgrade_effect.cpp
    ../src/ecs/combat/combat_system.cpp
    ../src/ecs/combat/projectile.cpp
    ../src/ecs/pathing/pathfinding.cpp
    ../src/ecs/pathing/world_collision.cpp
    ../src/ecs/behaviors/behavior_move.cpp
    ../src/ecs/behaviors/behavior_attack.cpp
    ../src/ecs/behaviors/behavior_attack_move.cpp
    ../src/ecs/behaviors/behavior_stop.cpp
    ../src/ecs/behaviors/behavior_hold_position.cpp
    ../src/ecs/behaviors/behavior_follow.cpp
    ../src/ecs/behaviors/behavior_patrol.cpp
    ../src/ecs/behaviors/behavior_stun.cpp
    ../src/ecs/behaviors/behavior_board_transport.cpp
    ../src/ecs/behaviors/behavior_build.cpp
    ../src/ecs/behaviors/behavior_harvest.cpp
    ../src/ecs/behaviors/behavior_return_resources.cpp
    ../src/ecs/behaviors/behavior_repair.cpp
    ../src/ecs/behaviors/behavior_spell.cpp
    ../src/ecs/abilities/ability_attack.cpp
    ../src/ecs/abilities/ability_move.cpp
    ../src/ecs/abilities/ability_hero.cpp
    ../src/ecs/abilities/ability_inventory.cpp
    ../src/ecs/abilities/ability_build.cpp
    ../src/ecs/abilities/ability_harvest.cpp
    ../src/ecs/abilities/ability_gold_mine.cpp
    ../src/ecs/abilities/ability_return_resources.cpp
    ../src/ecs/abilities/ability_queue.cpp
    ../src/ecs/abilities/ability_rally.cpp
    ../src/ecs/abilities/ability_revive_hero.cpp
    ../src/ecs/abilities/ability_upgrade.cpp
    ../src/ecs/abilities/ability_sell_items.cpp
    ../src/ecs/abilities/ability_spell_base.cpp
    ../src/ecs/abilities/ability_aura.cpp
    ../src/ecs/abilities/ability_repair.cpp
    ../src/ecs/abilities/ability_transport.cpp
    ../src/ecs/abilities/ability_root.cpp
    ../src/ecs/abilities/spells.cpp
    ../src/ecs/abilities/spells2.cpp
    ../src/ecs/abilities/auras.cpp
    ../src/ecs/abilities/ability_builder.cpp
)

target_include_directories(warsmash_tests PRIVATE ../src)

target_compile_definitions(warsmash_tests PRIVATE
    PROJECT_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

# StormLib (optional)
if(StormLib_FOUND)
    target_link_libraries(warsmash_tests PRIVATE StormLib::storm)
    if(PLATFORM_WINDOWS AND DEFINED VCPKG_TRIPLET)
        set(VCPKG_BIN "${VCPKG_INSTALLED_DIR}/${VCPKG_TRIPLET}/bin")
        foreach(DLL_FILE stormlib.dll zlib1.dll bz2.dll)
            set(DLL_PATH "${VCPKG_BIN}/${DLL_FILE}")
            if(EXISTS "${DLL_PATH}")
                add_custom_command(TARGET warsmash_tests POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${DLL_PATH}"
                        "$<TARGET_FILE_DIR:warsmash_tests>/"
                    COMMENT "Copying ${DLL_FILE} to test output"
                )
            endif()
        endforeach()
    endif()
else()
    message(STATUS "StormLib not found for tests")
    target_compile_definitions(warsmash_tests PRIVATE WITHOUT_STORMLIB=1)
endif()

add_test(NAME warsmash_tests COMMAND warsmash_tests)
