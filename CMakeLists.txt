# CMakeLists.txt

cmake_minimum_required(VERSION 3.10.0)
project(ebus VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14")
set(CMAKE_CXX_STANDARD 14)

# Ensure all libraries and executables are built with PIC for modern linkers
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Option to build legacy tests with custom main functions (not using Catch2's test discovery)
# commented out to default to ON since these tests are still being maintained and provide
# valuable insights into the development process. They can be excluded from the build if 
# desired by setting this option to OFF.
# option(EBUS_BUILD_LEGACY_TESTS "Build legacy development and insight tests" OFF)
set(EBUS_BUILD_LEGACY_TESTS ON CACHE BOOL "Build legacy tests" FORCE)

# Default to Debug build if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# Check if building for POSIX
if(UNIX AND NOT APPLE)
    add_definitions(-DPOSIX)
    set(POSIX ON)
endif()

include(FetchContent)
FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.4.0
)
FetchContent_MakeAvailable(Catch2)

enable_testing()

add_subdirectory(src/Ebus)
add_subdirectory(tests)
add_subdirectory(legacy)
add_subdirectory(tools)
