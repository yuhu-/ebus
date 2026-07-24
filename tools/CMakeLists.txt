# tools/CMakeLists.txt

include_directories(${PROJECT_SOURCE_DIR}/src/ebus)

add_executable(ebusread ebusread.cpp)
target_link_libraries(ebusread PRIVATE ebus)

add_executable(playground playground.cpp)
target_link_libraries(playground PRIVATE ebus)

add_executable(ebusproxy ebusproxy.cpp)
target_link_libraries(ebusproxy PRIVATE ebus)

