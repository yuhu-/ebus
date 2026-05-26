# src/ebus/CMakeLists.txt

# High-level Application Logic
set(APP_SOURCES
    app/client.cpp
    app/client_manager.cpp
    app/config.cpp
    app/config_validator.cpp
    app/controller.cpp
    app/device_manager.cpp
    app/device_scanner.cpp
    app/poll_manager.cpp
    app/scheduler.cpp
)

# Core Protocol Logic
set(CORE_SOURCES
    core/bus_monitor.cpp
    core/callbacks.cpp
    core/handler.cpp
    core/request.cpp
    core/types.cpp
)

# Data Models and Types
set(MODELS_SOURCES
    models/data_types.cpp
    models/device.cpp
)

# Platform Abstraction Layer
set(PLATFORM_SOURCES
)

if(EBUS_SIMULATION)
    list(APPEND PLATFORM_SOURCES platform/simulation/bus_simulation.cpp)
    list(APPEND PLATFORM_SOURCES platform/simulation/virtual_bus.cpp)
elseif(DEFINED ESP_PLATFORM)
    list(APPEND PLATFORM_SOURCES platform/esp/bus_esp.cpp)
elseif(POSIX)
    list(APPEND PLATFORM_SOURCES platform/posix/bus_posix.cpp)
endif()

# Utils
set(UTILS_SOURCES
    utils/json_utils.cpp
)

set(SOURCES
    ${APP_SOURCES} 
    ${CORE_SOURCES} 
    ${MODELS_SOURCES}
    ${PLATFORM_SOURCES}
    ${UTILS_SOURCES}
)

add_library(ebus ${SOURCES})

if(EBUS_SIMULATION)
    target_compile_definitions(ebus PUBLIC EBUS_SIMULATION=1)
else()
    target_compile_definitions(ebus PUBLIC EBUS_SIMULATION=0)
endif()

target_include_directories(ebus 
    PUBLIC ${CMAKE_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/detail
)

target_compile_options(ebus PRIVATE
  $<$<CXX_COMPILER_ID:GNU,Clang>:
    -Wall;-Wextra;-Wpedantic;-Wshadow;-Wnon-virtual-dtor;-Wformat=2;-fstack-protector-strong;-Wno-unused-function>
)

target_compile_options(ebus PUBLIC $<$<CONFIG:Debug>:-g;-O0>)
