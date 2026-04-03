# tests/CMakeLists.txt

# Minimum CMake version required by Catch2
cmake_minimum_required(VERSION 3.10)

# Find the Catch2 package. Version 3 is specified as per your Catch2 setup.
find_package(Catch2 3 REQUIRED)

# Enable CTest for test execution and include Catch.cmake for test discovery.
# Catch.cmake provides the `catch_discover_tests` function.
include(CTest)
include(Catch)

# This macro creates an executable, links it against Catch2's main,
# links it against 'ebus' library, sets necessary include directories,
# and then uses catch_discover_tests to find all TEST_CASEs within it.
macro(add_catch2_test_executable name source_file)
    add_executable(${name} ${source_file})
    # Link to Catch2's main and main ebus library
    target_link_libraries(${name} PRIVATE ebus Catch2::Catch2WithMain)
    # Add include directories for source and test utility headers
    target_include_directories(${name} PUBLIC
        ${PROJECT_SOURCE_DIR}/src/Ebus
        ${PROJECT_SOURCE_DIR}/tests # For TestUtils.hpp
    )
    # Discover all Catch2 tests in this executable
    catch_discover_tests(${name})
    add_test(NAME ${name} COMMAND ${name})
endmacro()

# Catch2 test executables using the macro

# High-level Application Logic
# add_catch2_test_executable(catch_scheduler App/catch_scheduler.cpp)
add_catch2_test_executable(catch_enhancedprotocol App/catch_enhancedprotocol.cpp)
add_catch2_test_executable(catch_client App/catch_client.cpp)
add_catch2_test_executable(catch_clientmanager App/catch_clientmanager.cpp)
# add_catch2_test_executable(catch_devicemanager App/catch_devicemanager.cpp)
# add_catch2_test_executable(catch_devicescanner App/catch_devicescanner.cpp)
add_catch2_test_executable(catch_pollmanager App/catch_pollmanager.cpp)
# add_catch2_test_executable(catch_controller App/catch_controller.cpp)

# Core Protocol Logic
add_catch2_test_executable(catch_sequence Core/catch_sequence.cpp)
add_catch2_test_executable(catch_telegram Core/catch_telegram.cpp)
add_catch2_test_executable(catch_request Core/catch_request.cpp)
add_catch2_test_executable(catch_handler Core/catch_handler.cpp)
add_catch2_test_executable(catch_bushandler Core/catch_bushandler.cpp)

# Data Models and Types
add_catch2_test_executable(catch_device Models/catch_device.cpp)
add_catch2_test_executable(catch_datatypes Models/catch_datatypes.cpp)

# Platform Abstraction Layer
add_catch2_test_executable(catch_bus Platform/catch_bus.cpp)
add_catch2_test_executable(catch_queue Platform/catch_queue.cpp)
add_catch2_test_executable(catch_servicethread Platform/catch_servicethread.cpp)

# Utilities
add_catch2_test_executable(catch_timingstats Utils/catch_timingstats.cpp)
add_catch2_test_executable(catch_common Utils/catch_common.cpp)
