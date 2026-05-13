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
    core/bus_monitor.cpp
    core/handler.cpp
    core/request.cpp
)

# Data Models and Types
set(MODELS_SOURCES
    models/data_types.cpp
    models/device.cpp
)

# Platform Abstraction Layer
set(PLATFORM_SOURCES
)

# Utils
set(UTILS_SOURCES
    utils/json_utils.cpp
)

if(POSIX AND NOT APPLE)
    list(APPEND PLATFORM_SOURCES platform/posix/bus_posix.cpp)
elseif(ESP_PLATFORM)
    list(APPEND PLATFORM_SOURCES platform/esp/bus_esp.cpp)
endif()

set(SOURCES
    ${APP_SOURCES} 
    ${CORE_SOURCES} 
    ${MODELS_SOURCES}
    ${PLATFORM_SOURCES}
    ${UTILS_SOURCES}
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
