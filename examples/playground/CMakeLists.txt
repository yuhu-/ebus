include_directories(${ebus_SOURCE_DIR}/src)
link_directories(${ebus_SOURCE_DIR}/src)

add_executable(playground playground.cpp)
target_link_libraries(playground ebus)
