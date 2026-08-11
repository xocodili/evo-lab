add_library(evolab_engine STATIC
  src/Camera.cpp
  src/Viewport.cpp
  src/FixedTimestepClock.cpp
  src/gl/GlContext.cpp
  src/gfx/ShaderProgram.cpp
  src/gfx/UiAssets.cpp
  src/gfx/UiFont.cpp
  src/gfx/TextOverlay.cpp
  src/gfx/TextPanel.cpp
)

target_include_directories(evolab_engine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(evolab_engine PRIVATE ${CMAKE_SOURCE_DIR}/third_party)
target_link_libraries(evolab_engine PUBLIC evolab_platform)

if(WIN32)
  target_link_libraries(evolab_engine PUBLIC opengl32)
else()
  find_package(OpenGL REQUIRED)
  target_link_libraries(evolab_engine PUBLIC OpenGL::GL)
endif()
