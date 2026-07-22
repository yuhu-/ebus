# CMakeLists.txt

cmake_minimum_required(VERSION 3.10.0)
project(ebus VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Ensure all libraries and executables are built with PIC for modern linkers
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Default to Debug build if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# Check if building for POSIX
if(UNIX AND NOT APPLE)
    add_definitions(-DPOSIX)
    set(POSIX ON)
endif()

# --- Feature Options ---
option(EBUS_SIMULATION "Enable virtual bus simulation mode" OFF)

# --- Library Options ---
option(EBUS_MINIMAL_DIAGNOSTICS "Enable minimal diagnostics" OFF)

# --- Memory Tuning Options ---
# These defaults match protocol_limits.hpp but can be overridden at build time.
# Component Layer
set(EBUS_TRANSITION_HISTORY_SIZE 10 CACHE STRING "Depth of FSM transition history tracking")
# Orchestration Layer
set(EBUS_CONTROLLER_STACK_SIZE 4096 CACHE STRING "Stack size for Controller thread")
set(EBUS_CONTROLLER_PRIORITY 5 CACHE STRING "Priority for Controller thread")
set(EBUS_BUS_STACK_SIZE 2048 CACHE STRING "Stack size for Bus")
set(EBUS_BUS_PRIORITY 15 CACHE STRING "Priority for Bus thread")
set(EBUS_BUS_SYN_STACK_SIZE 2048 CACHE STRING "Stack size for Bus SYN thread")
set(EBUS_BUS_SYN_PRIORITY 3 CACHE STRING "Priority for Bus SYN thread")
set(EBUS_CLIENT_MANAGER_STACK_SIZE 4096 CACHE STRING "Stack size for Client Manager thread")
set(EBUS_CLIENT_MANAGER_PRIORITY 12 CACHE STRING "Priority for Client Manager thread")
# Diagnostics Layer
set(EBUS_LOG_HISTORY_SIZE 60 CACHE STRING "Number of error entries to keep in memory")
set(EBUS_TRACE_HISTORY_SIZE 100 CACHE STRING "Number of trace events to keep in memory")
# Networking Layer
set(EBUS_MAX_CLIENTS 4 CACHE STRING "Maximum number of network bridge clients")
# Application Layer
set(EBUS_EVENT_QUEUE_SIZE 10 CACHE STRING "Size of the public event queue")
set(EBUS_REACTOR_QUEUE_SIZE 16 CACHE STRING "Size of the internal reactor queue")
set(EBUS_MAX_DEVICES 16 CACHE STRING "Maximum number of devices to manage")
set(EBUS_SCHEDULER_MAX_ITEMS 8 CACHE STRING "Size of the scheduler queue")
set(EBUS_POLL_MAX_ITEMS 64 CACHE STRING "Size of the poll queue")

set(EBUS_SIMULATION_VAL 0)
if(EBUS_SIMULATION)
    set(EBUS_SIMULATION_VAL 1)
endif()

if(EBUS_MINIMAL_DIAGNOSTICS)
    set(EBUS_TRANSITION_HISTORY_SIZE 1 CACHE STRING "Size of the internal reactor queue")
    set(EBUS_LOG_HISTORY_SIZE 1 CACHE STRING "Size of the diagnostic error log history")
    set(EBUS_TRACE_HISTORY_SIZE 1 CACHE STRING "Size of the protocol trace history")
endif()

# Apply definitions globally for the library, tests, and tools
add_compile_definitions(
    EBUS_SIMULATION=${EBUS_SIMULATION_VAL}
    # Component Layer
    EBUS_TRANSITION_HISTORY_SIZE=${EBUS_TRANSITION_HISTORY_SIZE}
    # Orchestration Layer
    EBUS_CONTROLLER_STACK_SIZE=${EBUS_CONTROLLER_STACK_SIZE}
    EBUS_CONTROLLER_PRIORITY=${EBUS_CONTROLLER_PRIORITY}
    EBUS_BUS_STACK_SIZE=${EBUS_BUS_STACK_SIZE}
    EBUS_BUS_PRIORITY=${EBUS_BUS_PRIORITY}
    EBUS_BUS_SYN_STACK_SIZE=${EBUS_BUS_SYN_STACK_SIZE}
    EBUS_BUS_SYN_PRIORITY=${EBUS_BUS_SYN_PRIORITY}
    EBUS_CLIENT_MANAGER_STACK_SIZE=${EBUS_CLIENT_MANAGER_STACK_SIZE}
    EBUS_CLIENT_MANAGER_PRIORITY=${EBUS_CLIENT_MANAGER_PRIORITY}
    # Bus LayerEBUS_CLIENT_MANAGER_STACK_SIZE
    EBUS_BUS_QUEUE_SIZE=${EBUS_BUS_QUEUE_SIZE}
    # Diagnostics Layer
    EBUS_MAX_LISTENERS=${EBUS_MAX_LISTENERS}
    EBUS_LOG_HISTORY_SIZE=${EBUS_LOG_HISTORY_SIZE}
    EBUS_TRACE_HISTORY_SIZE=${EBUS_TRACE_HISTORY_SIZE}
    # Networking Layer
    EBUS_MAX_CLIENTS=${EBUS_MAX_CLIENTS}
    # Application Layer
    EBUS_EVENT_QUEUE_SIZE=${EBUS_EVENT_QUEUE_SIZE}
    EBUS_REACTOR_QUEUE_SIZE=${EBUS_REACTOR_QUEUE_SIZE}
    EBUS_MAX_DEVICES=${EBUS_MAX_DEVICES}
    EBUS_SCHEDULER_MAX_ITEMS=${EBUS_SCHEDULER_MAX_ITEMS}
    EBUS_POLL_MAX_ITEMS=${EBUS_POLL_MAX_ITEMS}
)

if(EBUS_MINIMAL_DIAGNOSTICS)
    add_compile_definitions(EBUS_MINIMAL_DIAGNOSTICS)
endif()

add_subdirectory(src/ebus)
add_subdirectory(tools)

if(EBUS_SIMULATION)
    include(FetchContent)
    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG        v3.4.0
    )
    FetchContent_MakeAvailable(Catch2)

    enable_testing()
    
    add_subdirectory(tests)
    add_subdirectory(examples)
endif()
