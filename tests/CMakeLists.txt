include_directories(${ebus_SOURCE_DIR}/src/Ebus)
link_directories(${ebus_SOURCE_DIR}/src/Ebus)

add_executable(test_datatypes test_datatypes.cpp)
target_link_libraries(test_datatypes ebus)

add_executable(test_telegram test_telegram.cpp)
target_link_libraries(test_telegram ebus)

add_executable(test_handler test_handler.cpp)
target_link_libraries(test_handler ebus_posix_test)
target_compile_definitions(test_handler PRIVATE -DPOSIX_TEST)

add_executable(test_servicerunner test_servicerunner.cpp)
target_link_libraries(test_servicerunner ebus_posix_test)
target_compile_definitions(test_servicerunner PRIVATE -DPOSIX_TEST)

