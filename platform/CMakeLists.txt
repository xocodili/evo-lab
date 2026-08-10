include(FetchContent)

FetchContent_Declare(
  SDL2
  URL https://github.com/libsdl-org/SDL/releases/download/release-2.30.10/SDL2-2.30.10.tar.gz
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
set(SDL_SHARED_ENABLED_BY_DEFAULT OFF CACHE BOOL "" FORCE)
set(SDL_TEST OFF CACHE BOOL "" FORCE)
set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(SDL2)

add_library(evolab_platform STATIC
  src/SdlPlatform.cpp
)

target_include_directories(evolab_platform PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(evolab_platform PUBLIC SDL2::SDL2-static)
