# Catch2 Tests (Modern)
add_executable(catch_timingstats catch_timingstats.cpp)
target_link_libraries(catch_timingstats PRIVATE ebus Catch2::Catch2WithMain)

add_executable(catch_common catch_common.cpp)
target_link_libraries(catch_common PRIVATE ebus Catch2::Catch2WithMain)

# Legacy Tests (Manual main)
add_executable(test_timingstats test_timingstats.cpp)
target_link_libraries(test_timingstats PRIVATE ebus)

add_executable(test_common test_common.cpp)
target_link_libraries(test_common PRIVATE ebus)
