# tests/CMakeLists.txt

# Minimum CMake version required by Catch2
cmake_minimum_required(VERSION 3.10)

# Find the Catch2 package. Version 3 is specified as per your Catch2 setup.
find_package(Catch2 3 REQUIRED)

# Enable CTest for test execution and include Catch.cmake for test discovery.
# Catch.cmake provides the `catch_discover_tests` function.
include(CTest)
include(Catch)

# Define a macro to simplify adding Catch2 test executables.
# This macro creates an executable, links it against Catch2's main,
# links it against your 'ebus' library, sets necessary include directories,
# and then uses catch_discover_tests to find all TEST_CASEs within it.
macro(add_catch2_test_executable name source_file)
    add_executable(${name} ${source_file})
    # Link to Catch2's main and your main ebus library
    target_link_libraries(${name} PRIVATE Catch2::Catch2WithMain ebus)
    # Add include directories for your source and test utility headers
    target_include_directories(${name} PRIVATE
        ${PROJECT_SOURCE_DIR}/src/Ebus
        ${PROJECT_SOURCE_DIR}/tests # For TestUtils.hpp
    )
    # Discover all Catch2 tests in this executable
    catch_discover_tests(${name})
endmacro()

# Catch2 test executables using the macro
# App
add_catch2_test_executable(catch_enhancedprotocol App/catch_enhancedprotocol.cpp)
add_catch2_test_executable(catch_pollmanager App/catch_pollmanager.cpp)
# Core
add_catch2_test_executable(catch_sequence Core/catch_sequence.cpp)
add_catch2_test_executable(catch_telegram Core/catch_telegram.cpp)
add_catch2_test_executable(catch_request Core/catch_request.cpp)
add_catch2_test_executable(catch_handler Core/catch_handler.cpp)
add_catch2_test_executable(catch_bushandler Core/catch_bushandler.cpp)
# Models
add_catch2_test_executable(catch_device Models/catch_device.cpp)
add_catch2_test_executable(catch_datatypes Models/catch_datatypes.cpp)
# Platform
add_catch2_test_executable(catch_bus Platform/catch_bus.cpp)
add_catch2_test_executable(catch_queue Platform/catch_queue.cpp)
add_catch2_test_executable(catch_servicethread Platform/catch_servicethread.cpp)
# Utils
add_catch2_test_executable(catch_timingstats Utils/catch_timingstats.cpp)
add_catch2_test_executable(catch_common Utils/catch_common.cpp)

# Legacy tests with custom main functions (not using Catch2's test discovery)
include_directories(${PROJECT_SOURCE_DIR}/src/Ebus)
include_directories(${PROJECT_SOURCE_DIR}/tests)

add_executable(test_bushandler Core/test_bushandler.cpp)
target_link_libraries(test_bushandler PRIVATE ebus)

add_executable(test_request Core/test_request.cpp)
target_link_libraries(test_request PRIVATE ebus)

# add_test(NAME test_bushandler COMMAND test_bushandler)