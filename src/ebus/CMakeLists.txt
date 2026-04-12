# src/ebus/CMakeLists.txt

# High-level Application Logic
set(APP_SOURCES
    app/controller.cpp
    app/client.cpp
    app/client_manager.cpp
    app/device_manager.cpp
    app/device_scanner.cpp
    app/poll_manager.cpp
    app/scheduler.cpp
)

# Core Protocol Logic
set(CORE_SOURCES
    core/handler.cpp
    core/request.cpp
    core/sequence.cpp
    core/telegram.cpp
)

# Data Models and Types
set(MODELS_SOURCES
    models/data_types.cpp
    models/device.cpp
    models/manufacturers.cpp
)

# Platform Abstraction Layer
set(PLATFORM_SOURCES
    platform/service_thread.cpp
    platform/system.cpp
)

if(POSIX AND NOT APPLE)
    list(APPEND PLATFORM_SOURCES platform/posix/bus_posix.cpp)
    list(APPEND PLATFORM_SOURCES platform/posix/virtual_line.cpp)
elseif(ESP32)
    list(APPEND PLATFORM_SOURCES platform/freertos/bus_freertos.cpp)
endif()

# Utilities
set(UTIL_SOURCES
    utils/utils.cpp
    utils/utils_internal.cpp
    utils/timing_stats.cpp
)

set(SOURCES
    ${APP_SOURCES} 
    ${CORE_SOURCES} 
    ${MODELS_SOURCES}
    ${PLATFORM_SOURCES}
    ${UTIL_SOURCES} 
)

add_library(ebus ${SOURCES})

target_include_directories(ebus 
    PUBLIC ${CMAKE_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_options(ebus PRIVATE
  $<$<CXX_COMPILER_ID:GNU,Clang>:
    -Wall;-Wextra;-Wpedantic;-Wshadow;-Wnon-virtual-dtor;-Wformat=2;-fstack-protector-strong;-Wno-unused-function>
)

target_compile_options(ebus PUBLIC $<$<CONFIG:Debug>:-g;-O0>)
