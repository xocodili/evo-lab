include(FetchContent)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build Catch2 statically" FORCE)

FetchContent_Declare(
  Catch2
  URL https://github.com/catchorg/Catch2/archive/refs/tags/v3.5.4.zip
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(Catch2)

add_executable(evo-lab-tests
  unit/test_heightmap.cpp
  unit/test_tide.cpp
  unit/test_wetness.cpp
  unit/test_barren_world.cpp
  unit/test_camera.cpp
  unit/test_engine_scaffold.cpp
  unit/test_kinematics.cpp
  unit/test_energon.cpp
  unit/test_energon_information.cpp
  unit/test_stem_cell.cpp
  unit/test_skeleton.cpp
  unit/test_actuator.cpp
  unit/test_nom.cpp
  unit/test_computer.cpp
  unit/test_stem_equilibrium.cpp
  unit/test_cloaca_perceptor.cpp
  unit/test_perceptor_diurnal.cpp
  unit/test_cloaca_palette.cpp
  unit/test_neuron_signal.cpp
  unit/test_neuron_trust.cpp
  unit/test_energon_conveyance.cpp
  unit/test_regulation_satiety.cpp
  unit/test_tank_marathon.cpp
  unit/test_locomotion_diagnostic.cpp
  unit/test_hgt_insertion.cpp
  unit/test_parthenogenesis.cpp
  unit/test_stem_binding.cpp
  unit/test_debug_session.cpp
  unit/test_cornucopia_chemotaxis.cpp
  unit/test_mouth_taste.cpp
  unit/test_coordinator.cpp
  unit/test_camp_tumble.cpp
  unit/test_sprite_animator.cpp
  unit/test_nursery_mutant.cpp
  unit/test_chaos.cpp
  unit/test_tide_advection.cpp
  unit/test_hydrology.cpp
  unit/test_water_column.cpp
)

target_link_libraries(evo-lab-tests PRIVATE evolab_sim evolab_game evolab_engine Catch2::Catch2WithMain)

if(WIN32)
  target_link_libraries(evo-lab-tests PRIVATE opengl32 imm32 version setupapi winmm)
endif()

include(Catch)
catch_discover_tests(evo-lab-tests DISCOVERY_MODE PRE_TEST)

# Tiered regression gates — see tests/TEST_PLAN.md
add_test(
  NAME tier3_marathon
  COMMAND evo-lab-tests "[tier3][marathon]"
)
set_tests_properties(tier3_marathon PROPERTIES LABELS "tier3;marathon")

add_test(
  NAME tier2_nursery
  COMMAND evo-lab-tests "[nursery]"
)
set_tests_properties(tier2_nursery PROPERTIES LABELS "tier2;nursery")

add_test(
  NAME tier1_diagnostic
  COMMAND evo-lab-tests "~[nursery]~[marathon]~[ultra-marathon]~[long]~[drift]~[optional]"
)
set_tests_properties(tier1_diagnostic PROPERTIES LABELS "tier1;diagnostic")

add_test(
  NAME smoke_headless
  COMMAND evo-lab --headless --frames 120 --seed 42 --exit
)

add_test(
  NAME smoke_headless_debug_session
  COMMAND evo-lab --headless --debug 250 --frames 400 --seed 42 --exit
)
