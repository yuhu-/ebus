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

# --- Memory Tuning Options ---
# These defaults match protocol_limits.hpp but can be overridden at build time.
set(EBUS_FSM_HISTORY_SIZE 10 CACHE STRING "Depth of FSM transition history tracking")
set(EBUS_STACK_SIZE_HIGH 4096 CACHE STRING "Stack size for heavy tasks (Controller)")
set(EBUS_STACK_SIZE_MED 3072 CACHE STRING "Stack size for medium tasks (Clients)")
set(EBUS_STACK_SIZE_LOW 1536 CACHE STRING "Stack size for protocol tasks (Bus/Handler)")
set(EBUS_BUS_QUEUE_SIZE 512 CACHE STRING "Size of the physical bus event queue")
set(EBUS_MAX_LISTENERS 4 CACHE STRING "Maximum number of internal bus listeners")
set(EBUS_LOG_HISTORY_SIZE 60 CACHE STRING "Number of error entries to keep in memory")
set(EBUS_TRACE_HISTORY_SIZE 100 CACHE STRING "Number of trace events to keep in memory")
set(EBUS_MAX_CLIENTS 4 CACHE STRING "Maximum number of network bridge clients")
set(EBUS_MAX_MANUAL_QUEUE 16 CACHE STRING "Maximum pending manual scan requests")
set(EBUS_SCHEDULER_MAX_ITEMS 128 CACHE STRING "Maximum items in the priority scheduler")
set(EBUS_POLL_MAX_ITEMS 128 CACHE STRING "Maximum items in the poll manager")


# Apply definitions globally for the library, tests, and tools
add_compile_definitions(
    EBUS_FSM_HISTORY_SIZE=${EBUS_FSM_HISTORY_SIZE}
    EBUS_STACK_SIZE_HIGH=${EBUS_STACK_SIZE_HIGH}
    EBUS_STACK_SIZE_MED=${EBUS_STACK_SIZE_MED}
    EBUS_STACK_SIZE_LOW=${EBUS_STACK_SIZE_LOW}
    EBUS_BUS_QUEUE_SIZE=${EBUS_BUS_QUEUE_SIZE}
    EBUS_MAX_LISTENERS=${EBUS_MAX_LISTENERS}
    EBUS_LOG_HISTORY_SIZE=${EBUS_LOG_HISTORY_SIZE}
    EBUS_TRACE_HISTORY_SIZE=${EBUS_TRACE_HISTORY_SIZE}
    EBUS_MAX_CLIENTS=${EBUS_MAX_CLIENTS}
    EBUS_MAX_MANUAL_QUEUE=${EBUS_MAX_MANUAL_QUEUE}
    EBUS_SCHEDULER_MAX_ITEMS=${EBUS_SCHEDULER_MAX_ITEMS}
    EBUS_POLL_MAX_ITEMS=${EBUS_POLL_MAX_ITEMS}
)

include(FetchContent)
FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.4.0
)
FetchContent_MakeAvailable(Catch2)

enable_testing()

add_subdirectory(src/ebus)
add_subdirectory(tests)
add_subdirectory(tools)
add_subdirectory(examples)
