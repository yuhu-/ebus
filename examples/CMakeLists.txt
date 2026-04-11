# examples/CMakeLists.txt

include_directories(${PROJECT_SOURCE_DIR}/src/Ebus)

add_executable(example_virtual_bus_readonly_client example_virtual_bus_readonly_client.cpp)
target_link_libraries(example_virtual_bus_readonly_client PRIVATE ebus)

