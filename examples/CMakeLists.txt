# examples/CMakeLists.txt

include_directories(${PROJECT_SOURCE_DIR}/src/Ebus)

add_executable(multiple_controllers_on_virtual_bus multiple_controllers_on_virtual_bus.cpp)
target_link_libraries(multiple_controllers_on_virtual_bus ebus)

