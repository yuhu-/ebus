# Catch2 Tests (Modern)
add_executable(catch_device catch_device.cpp)
target_link_libraries(catch_device PRIVATE ebus Catch2::Catch2WithMain)

add_executable(catch_datatypes catch_datatypes.cpp)
target_link_libraries(catch_datatypes PRIVATE ebus Catch2::Catch2WithMain)

# Legacy Tests (Manual main)
add_executable(test_device test_device.cpp)
target_link_libraries(test_device PRIVATE ebus)

add_executable(test_datatypes test_datatypes.cpp)
target_link_libraries(test_datatypes PRIVATE ebus)