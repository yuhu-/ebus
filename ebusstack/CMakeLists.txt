include_directories(${ebus_SOURCE_DIR}/src)
link_directories(${ebus_SOURCE_DIR}/src)

add_library(ebusstack EbusStack.cpp Device.cpp)

add_executable(demo_ebusstack demo_ebusstack.cpp)
target_link_libraries(demo_ebusstack ebusstack ebus)
