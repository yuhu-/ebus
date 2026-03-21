set(SOURCES
    Common.cpp
    Controller.cpp
    Datatypes.cpp
    Handler.cpp
    Request.cpp
    Sequence.cpp
    Telegram.cpp
    TimingStats.cpp
)

if(POSIX)
    list(APPEND SOURCES Posix/BusPosix.cpp)
endif()

add_library(ebus ${SOURCES})

target_include_directories(ebus PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_options(ebus PRIVATE
  $<$<CXX_COMPILER_ID:GNU,Clang>:
    -Wall;-Wextra;-Wpedantic;-Wshadow;-Wnon-virtual-dtor;-Wformat=2;-fstack-protector-strong>
)

target_compile_options(ebus PUBLIC $<$<CONFIG:Debug>:-g;-O0>)
