add_executable(test_datatypes test_datatypes.cpp)
target_link_libraries(test_datatypes ebus)

add_executable(test_common test_common.cpp)
target_link_libraries(test_common ebus)

add_executable(test_sequence test_sequence.cpp)
target_link_libraries(test_sequence ebus)

add_executable(test_telegram test_telegram.cpp)
target_link_libraries(test_telegram ebus)

add_executable(test_request test_request.cpp)
target_link_libraries(test_request ebus)

add_executable(test_bus test_bus.cpp)
target_link_libraries(test_bus ebus)

add_executable(test_handler test_handler.cpp)
target_link_libraries(test_handler ebus)

add_executable(test_bushandler test_bushandler.cpp)
target_link_libraries(test_bushandler ebus)

add_executable(test_queue test_queue.cpp)
target_link_libraries(test_queue ebus)
