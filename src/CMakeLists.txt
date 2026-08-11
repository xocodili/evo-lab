add_library(evolab_game STATIC
  game/TerrainMesh.cpp
  game/GameShaders.cpp
  game/OrganismDrawer.cpp
  game/OrganismInspector.cpp
  game/GameRenderer.cpp
  game/GameHud.cpp
  game/CellPicker.cpp
  game/GameInspector.cpp
)

target_include_directories(evolab_game PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(evolab_game PUBLIC evolab_sim evolab_engine)

add_library(evolab_sim STATIC
  sim/Noise.cpp
  sim/Heightmap.cpp
  sim/Tide.cpp
  sim/BarrenWorld.cpp
  sim/DayCycle.cpp
  sim/Energon.cpp
  sim/EnergonString.cpp
  sim/EnergonStats.cpp
  sim/SimDiagnostics.cpp
  sim/SimConfig.cpp
  sim/Organism.cpp
  sim/OrganismDetail.cpp
  sim/OrganismPerceptor.cpp
  sim/OrganismKinematics.cpp
  sim/OrganismFactories.cpp
  sim/NeuronTick.cpp
  sim/NeuralAxon.cpp
  sim/Chaos.cpp
  sim/CellPopulation.cpp
  sim/TideAdvection.cpp
  sim/Hydrology.cpp
  sim/WaterColumn.cpp
)

target_include_directories(evolab_sim PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(evo-lab
  app/main.cpp
  app/CliArgs.cpp
  app/SmokeTest.cpp
  app/VisualApp.cpp
)
target_link_libraries(evo-lab PRIVATE evolab_sim evolab_game evolab_engine evolab_platform)

add_custom_command(
  TARGET evo-lab POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:evo-lab>/assets/fonts"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${CMAKE_SOURCE_DIR}/assets/fonts/LexendDeca-Regular.ttf"
          "$<TARGET_FILE_DIR:evo-lab>/assets/fonts/LexendDeca-Regular.ttf"
  COMMENT "Copy Lexend Deca font next to evo-lab.exe"
)

if(WIN32)
  target_link_libraries(evo-lab PRIVATE imm32 version setupapi winmm)
endif()
