include_directories(${ebus_SOURCE_DIR}/src/Ebus)
link_directories(${ebus_SOURCE_DIR}/src/Ebus)

add_executable(test_datatypes test_datatypes.cpp)
target_link_libraries(test_datatypes ebus)

add_executable(test_telegram test_telegram.cpp)
target_link_libraries(test_telegram ebus)

add_executable(test_request test_request.cpp)
target_link_libraries(test_request ebus)

add_executable(test_handler test_handler.cpp)
target_link_libraries(test_handler ebus)

add_executable(test_bushandler test_bushandler.cpp)
target_link_libraries(test_bushandler ebus)

