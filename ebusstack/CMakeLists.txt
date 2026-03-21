add_library(ebusstack EbusStack.cpp Device.cpp)
target_link_libraries(ebusstack ebus)

add_executable(demo_ebusstack demo_ebusstack.cpp)
target_link_libraries(demo_ebusstack ebusstack)
