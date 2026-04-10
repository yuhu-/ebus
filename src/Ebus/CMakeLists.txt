# src/Ebus/CMakeLists.txt

# High-level Application Logic
set(APP_SOURCES
    App/Controller.cpp
    App/Client.cpp
    App/ClientManager.cpp
    App/DeviceManager.cpp
    App/DeviceScanner.cpp
    App/PollManager.cpp
    App/Scheduler.cpp
)

# Core Protocol Logic
set(CORE_SOURCES
    Core/Sequence.cpp
    Core/Telegram.cpp
    Core/Handler.cpp
    Core/Request.cpp
)

# Data Models and Types
set(MODELS_SOURCES
    Models/Datatypes.cpp
    Models/Device.cpp
    Models/Manufacturers.cpp
)

# Platform Abstraction Layer
set(PLATFORM_SOURCES
    Platform/ServiceThread.cpp
)

if(POSIX AND NOT APPLE)
    list(APPEND PLATFORM_SOURCES Platform/Posix/BusPosix.cpp)
    list(APPEND PLATFORM_SOURCES Platform/Posix/VirtualLine.cpp)
elseif(ESP32)
    list(APPEND PLATFORM_SOURCES Platform/FreeRTOS/BusFreeRtos.cpp)
endif()

# Utilities
set(UTIL_SOURCES
    Utils/Common.cpp
    Utils/TimingStats.cpp
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
