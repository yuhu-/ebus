# legacy/CMakeLists.txt

# Legacy tests with custom main functions (not using Catch2's test discovery)

macro(add_legacy_test_executable name source_file)
    add_executable(${name} ${source_file})
    
    # Link to main ebus library
    target_link_libraries(${name} PRIVATE ebus)

    # Add include directories for your source and test utility headers
    target_include_directories(${name} PRIVATE
        ${PROJECT_SOURCE_DIR}/src/ebus
        ${PROJECT_SOURCE_DIR}/tests # For test_utils.hpp
    )
    
    # Add a test to CTest for this executable
    add_test(NAME ${name} COMMAND ${name})
endmacro()

# High-level Application Logic
add_legacy_test_executable(legacy_scheduler app/legacy_scheduler.cpp)
add_legacy_test_executable(legacy_enhanced_protocol app/legacy_enhanced_protocol.cpp)
add_legacy_test_executable(legacy_client app/legacy_client.cpp)
add_legacy_test_executable(legacy_client_manager app/legacy_client_manager.cpp)
add_legacy_test_executable(legacy_device_manager app/legacy_device_manager.cpp)
add_legacy_test_executable(legacy_device_scanner app/legacy_device_scanner.cpp)
add_legacy_test_executable(legacy_poll_manager app/legacy_poll_manager.cpp)
add_legacy_test_executable(legacy_controller app/legacy_controller.cpp)

# Core Protocol Logic
add_legacy_test_executable(legacy_sequence core/legacy_sequence.cpp)
add_legacy_test_executable(legacy_telegram core/legacy_telegram.cpp)
add_legacy_test_executable(legacy_request core/legacy_request.cpp)
add_legacy_test_executable(legacy_handler core/legacy_handler.cpp)
add_legacy_test_executable(legacy_bus_handler core/legacy_bus_handler.cpp)

# Data Models and Types
add_legacy_test_executable(legacy_device models/legacy_device.cpp)
add_legacy_test_executable(legacy_data_types models/legacy_data_types.cpp)

# Platform Abstraction Layer
add_legacy_test_executable(legacy_bus platform/legacy_bus.cpp)
add_legacy_test_executable(legacy_queue platform/legacy_queue.cpp)
add_legacy_test_executable(legacy_service_thread platform/legacy_service_thread.cpp)

# Utilities
add_legacy_test_executable(legacy_timing_stats utils/legacy_timing_stats.cpp)
add_legacy_test_executable(legacy_common utils/legacy_common.cpp)
