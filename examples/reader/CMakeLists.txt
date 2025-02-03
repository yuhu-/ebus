include_directories(${ebus_SOURCE_DIR}/src)
link_directories(${ebus_SOURCE_DIR}/src)

add_executable(reader reader.cpp)
target_link_libraries(reader ebus)
