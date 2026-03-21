file(GLOB SOURCES "*.cpp")

file(GLOB POSIX_SOURCES "Posix/*.cpp")

list(APPEND SOURCES ${POSIX_SOURCES})

add_library(ebus ${SOURCES})

target_compile_options(ebus PRIVATE
  $<$<CXX_COMPILER_ID:GNU,Clang>:
    -Wall;-Wextra;-Wpedantic;-Wshadow;-Wnon-virtual-dtor;-Wformat=2;-fstack-protector-strong>
)
