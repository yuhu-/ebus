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
add_catch2_test_executable(test_scheduler app/test_scheduler.cpp)
add_catch2_test_executable(test_enhanced_protocol app/test_enhanced_protocol.cpp)
add_catch2_test_executable(test_client app/test_client.cpp)
add_catch2_test_executable(test_client_manager app/test_client_manager.cpp)
add_catch2_test_executable(test_device_manager app/test_device_manager.cpp)
add_catch2_test_executable(test_device_scanner app/test_device_scanner.cpp)
add_catch2_test_executable(test_poll_manager app/test_poll_manager.cpp)
add_catch2_test_executable(test_controller app/test_controller.cpp)
add_catch2_test_executable(test_config_validator app/test_config_validator.cpp)
add_catch2_test_executable(test_virtual_bus app/test_virtual_bus.cpp)

# Core Protocol Logic
add_catch2_test_executable(test_sequence core/test_sequence.cpp)
add_catch2_test_executable(test_telegram core/test_telegram.cpp)
add_catch2_test_executable(test_request core/test_request.cpp)
add_catch2_test_executable(test_handler core/test_handler.cpp)
add_catch2_test_executable(test_bus_handler core/test_bus_handler.cpp)

# Data Models and Types
add_catch2_test_executable(test_device models/test_device.cpp)
add_catch2_test_executable(test_data_types models/test_data_types.cpp)

# Platform Abstraction Layer
add_catch2_test_executable(test_bus platform/test_bus.cpp)
add_catch2_test_executable(test_queue platform/test_queue.cpp)
add_catch2_test_executable(test_service_thread platform/test_service_thread.cpp)

# Utilities
add_catch2_test_executable(test_timing_stats utils/test_timing_stats.cpp)
add_catch2_test_executable(test_json_utils utils/test_json_utils.cpp)
add_catch2_test_executable(test_delegate utils/test_delegate.cpp)
add_catch2_test_executable(test_utils utils/test_utils.cpp)
add_catch2_test_executable(test_logger utils/test_logger.cpp)
add_catch2_test_executable(test_circular_buffer utils/test_circular_buffer.cpp)
add_catch2_test_executable(test_format_float utils/test_format_float.cpp)
