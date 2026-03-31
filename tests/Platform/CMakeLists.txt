# Catch2 Tests (Modern)
add_executable(catch_bus catch_bus.cpp)
target_link_libraries(catch_bus PRIVATE ebus Catch2::Catch2WithMain)

# Legacy Tests (Manual main)
add_executable(test_bus test_bus.cpp)
target_link_libraries(test_bus PRIVATE ebus)

add_executable(test_queue test_queue.cpp)
target_link_libraries(test_queue PRIVATE ebus)

add_executable(test_servicethread test_servicethread.cpp)
target_link_libraries(test_servicethread PRIVATE ebus)
