# Catch2 Tests (Modern)
add_executable(catch_enhancedprotocol catch_enhancedprotocol.cpp)
target_link_libraries(catch_enhancedprotocol PRIVATE ebus Catch2::Catch2WithMain)

# Legacy Tests (Manual main)
add_executable(test_scheduler test_scheduler.cpp)
target_link_libraries(test_scheduler PRIVATE ebus)

add_executable(test_devicemanager test_devicemanager.cpp)
target_link_libraries(test_devicemanager PRIVATE ebus)

add_executable(test_devicescanner test_devicescanner.cpp)
target_link_libraries(test_devicescanner PRIVATE ebus)

add_executable(test_client test_client.cpp)
target_link_libraries(test_client PRIVATE ebus)

add_executable(test_pollmanager test_pollmanager.cpp)
target_link_libraries(test_pollmanager PRIVATE ebus)

add_executable(test_enhancedprotocol test_enhancedprotocol.cpp)
target_link_libraries(test_enhancedprotocol PRIVATE ebus)

add_executable(test_clientmanager test_clientmanager.cpp)
target_link_libraries(test_clientmanager PRIVATE ebus)

add_executable(test_controller test_controller.cpp)
target_link_libraries(test_controller PRIVATE ebus)