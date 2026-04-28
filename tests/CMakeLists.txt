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
        ${PROJECT_SOURCE_DIR}/src/ebus
        ${PROJECT_SOURCE_DIR}/tests # For test_utils.hpp
    )
    
    # Discover all Catch2 tests in this executable
    catch_discover_tests(${name})
endmacro()

# Catch2 test executables using the macro

# High-level Application Logic
add_catch2_test_executable(catch_scheduler app/catch_scheduler.cpp)
add_catch2_test_executable(catch_enhanced_protocol app/catch_enhanced_protocol.cpp)
add_catch2_test_executable(catch_client app/catch_client.cpp)
add_catch2_test_executable(catch_client_manager app/catch_client_manager.cpp)
add_catch2_test_executable(catch_client_manager_mock app/catch_client_manager_mock.cpp)
add_catch2_test_executable(catch_device_manager app/catch_device_manager.cpp)
add_catch2_test_executable(catch_device_scanner app/catch_device_scanner.cpp)
add_catch2_test_executable(catch_poll_manager app/catch_poll_manager.cpp)
add_catch2_test_executable(catch_controller app/catch_controller.cpp)

# Core Protocol Logic
add_catch2_test_executable(catch_sequence core/catch_sequence.cpp)
add_catch2_test_executable(catch_telegram core/catch_telegram.cpp)
add_catch2_test_executable(catch_request core/catch_request.cpp)
add_catch2_test_executable(catch_handler core/catch_handler.cpp)
add_catch2_test_executable(catch_bus_handler core/catch_bus_handler.cpp)
add_catch2_test_executable(catch_telemetry core/catch_telemetry.cpp)

# Data Models and Types
add_catch2_test_executable(catch_device models/catch_device.cpp)
add_catch2_test_executable(catch_data_types models/catch_data_types.cpp)

# Platform Abstraction Layer
add_catch2_test_executable(catch_bus platform/catch_bus.cpp)
add_catch2_test_executable(catch_queue platform/catch_queue.cpp)
add_catch2_test_executable(catch_service_thread platform/catch_service_thread.cpp)

# Utilities
add_catch2_test_executable(catch_timing_stats utils/catch_timing_stats.cpp)
add_catch2_test_executable(catch_utils utils/catch_utils.cpp)
add_catch2_test_executable(catch_json_utils utils/catch_json_utils.cpp)
