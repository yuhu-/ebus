file(GLOB SOURCES "*.cpp")

file(GLOB POSIX_SOURCES "Posix/*.cpp")

list(APPEND SOURCES ${POSIX_SOURCES})

add_library(ebus ${SOURCES})