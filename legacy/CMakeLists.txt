# legacy/CMakeLists.txt

# Legacy tests with custom main functions (not using Catch2's test discovery)

macro(add_legacy_test_executable name source_file)
    add_executable(${name} ${source_file})
    
    # Link to main ebus library
    target_link_libraries(${name} PRIVATE ebus)

    # Add include directories for your source and test utility headers
    target_include_directories(${name} PRIVATE
        ${PROJECT_SOURCE_DIR}/src/Ebus
        ${PROJECT_SOURCE_DIR}/tests # For TestUtils.hpp
    )
    
    # Add a test to CTest for this executable
    add_test(NAME ${name} COMMAND ${name})
endmacro()

# High-level Application Logic
add_legacy_test_executable(legacy_scheduler App/legacy_scheduler.cpp)
add_legacy_test_executable(legacy_enhancedprotocol App/legacy_enhancedprotocol.cpp)
add_legacy_test_executable(legacy_client App/legacy_client.cpp)
add_legacy_test_executable(legacy_clientmanager App/legacy_clientmanager.cpp)
add_legacy_test_executable(legacy_devicemanager App/legacy_devicemanager.cpp)
add_legacy_test_executable(legacy_devicescanner App/legacy_devicescanner.cpp)
add_legacy_test_executable(legacy_pollmanager App/legacy_pollmanager.cpp)
add_legacy_test_executable(legacy_controller App/legacy_controller.cpp)

# Core Protocol Logic
add_legacy_test_executable(legacy_sequence Core/legacy_sequence.cpp)
add_legacy_test_executable(legacy_telegram Core/legacy_telegram.cpp)
add_legacy_test_executable(legacy_request Core/legacy_request.cpp)
add_legacy_test_executable(legacy_handler Core/legacy_handler.cpp)
add_legacy_test_executable(legacy_bushandler Core/legacy_bushandler.cpp)

# Data Models and Types
add_legacy_test_executable(legacy_device Models/legacy_device.cpp)
add_legacy_test_executable(legacy_datatypes Models/legacy_datatypes.cpp)

# Platform Abstraction Layer
add_legacy_test_executable(legacy_bus Platform/legacy_bus.cpp)
add_legacy_test_executable(legacy_queue Platform/legacy_queue.cpp)
add_legacy_test_executable(legacy_servicethread Platform/legacy_servicethread.cpp)

# Utilities
add_legacy_test_executable(legacy_timingstats Utils/legacy_timingstats.cpp)
add_legacy_test_executable(legacy_common Utils/legacy_common.cpp)
