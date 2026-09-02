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
  sim/EnergonInformation.cpp
  sim/EnergonSpatialIndex.cpp
  sim/EnergonTasteSensory.cpp
  sim/EnergonStats.cpp
  sim/SimDiagnostics.cpp
  sim/SimConfig.cpp
  sim/Organism.cpp
  sim/OrganismDetail.cpp
  sim/OrganismActuator.cpp
  sim/OrganismMouth.cpp
  sim/OrganismNeuron.cpp
  sim/OrganismPerceptor.cpp
  sim/NeuronSignal.cpp
  sim/NeuronTrust.cpp
  sim/NeuronFuel.cpp
  sim/CloacaSignal.cpp
  sim/CampTopology.cpp
  sim/NeuronStem.cpp
  sim/NeuronMusculature.cpp
  sim/EnergonConveyance.cpp
  sim/OrganismComputer.cpp
  sim/NeuronCoordinator.cpp
  sim/OrganismKinematics.cpp
  sim/OrganismFactories.cpp
  sim/NeuronTick.cpp
  sim/NeuralAxon.cpp
  sim/Chaos.cpp
  sim/OrganismHgt.cpp
  sim/WorldBinding.cpp
  sim/StemBinding.cpp
  sim/OrganismParthenogenesis.cpp
  sim/OrganismFeedbagOracle.cpp
  sim/CellPopulation.cpp
  sim/SessionDebugLog.cpp
  sim/CampTraceLog.cpp
  sim/TideAdvection.cpp
  sim/Hydrology.cpp
  sim/WaterColumn.cpp
)

target_include_directories(evolab_sim PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(evolab_sim PUBLIC evolab_engine)

add_executable(evo-lab
  app/main.cpp
  app/CliArgs.cpp
  app/SmokeTest.cpp
  app/StartupTrace.cpp
  app/VisualApp.cpp
)
target_link_libraries(evo-lab PRIVATE evolab_sim evolab_game evolab_engine evolab_platform)

add_custom_command(
  TARGET evo-lab POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:evo-lab>/assets/fonts"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${CMAKE_SOURCE_DIR}/assets/fonts/LexendDeca-Regular.ttf"
          "$<TARGET_FILE_DIR:evo-lab>/assets/fonts/LexendDeca-Regular.ttf"
  COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:evo-lab>/resources/sprites"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${CMAKE_SOURCE_DIR}/resources/sprites/mouth_sprites.json"
          "$<TARGET_FILE_DIR:evo-lab>/resources/sprites/mouth_sprites.json"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${CMAKE_SOURCE_DIR}/resources/sprites/mouth_sprites.png"
          "$<TARGET_FILE_DIR:evo-lab>/resources/sprites/mouth_sprites.png"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${CMAKE_SOURCE_DIR}/resources/sprites/perceptor_sprites.json"
          "$<TARGET_FILE_DIR:evo-lab>/resources/sprites/perceptor_sprites.json"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${CMAKE_SOURCE_DIR}/resources/sprites/perceptor_sprites.png"
          "$<TARGET_FILE_DIR:evo-lab>/resources/sprites/perceptor_sprites.png"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${CMAKE_SOURCE_DIR}/resources/sprites/actuator_sprites.json"
          "$<TARGET_FILE_DIR:evo-lab>/resources/sprites/actuator_sprites.json"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${CMAKE_SOURCE_DIR}/resources/sprites/actuator_sprites.png"
          "$<TARGET_FILE_DIR:evo-lab>/resources/sprites/actuator_sprites.png"
  COMMENT "Copy UI font and sprite resources next to evo-lab.exe"
)

if(WIN32)
  target_link_libraries(evo-lab PRIVATE imm32 version setupapi winmm)
endif()
