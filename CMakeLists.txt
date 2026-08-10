cmake_minimum_required(VERSION 3.20)
project(evo-lab VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(EVOLAB_BUILD_TESTS "Build unit tests" ON)

add_subdirectory(platform)
add_subdirectory(engine)
add_subdirectory(src)

if(EVOLAB_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
