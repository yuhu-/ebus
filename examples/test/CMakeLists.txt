include_directories(${ebus_SOURCE_DIR}/src)
link_directories(${ebus_SOURCE_DIR}/src)

add_executable(test_datatypes test_datatypes.cpp)
target_link_libraries(test_datatypes ebus)

add_executable(test_telegram test_telegram.cpp)
target_link_libraries(test_telegram ebus)

add_executable(test_ebushandler test_ebushandler.cpp)
target_link_libraries(test_ebushandler ebus)
