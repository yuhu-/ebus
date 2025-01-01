include_directories(${ebus_SOURCE_DIR}/src)
link_directories(${ebus_SOURCE_DIR}/src)

add_library(ebusstack EbusStack.cpp Device.cpp)
