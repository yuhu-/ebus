include_directories(${ebus_SOURCE_DIR}/src)
include_directories(${ebus_SOURCE_DIR}/examples/ebusstack/src)

link_directories(${ebus_SOURCE_DIR}/src)
link_directories(${ebus_SOURCE_DIR}/examples/ebusstack/src)

add_executable(demo_ebusstack demo_ebusstack.cpp)
target_link_libraries(demo_ebusstack ebusstack ebus)
