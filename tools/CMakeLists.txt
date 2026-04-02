# tools/CMakeLists.txt

include_directories(${PROJECT_SOURCE_DIR}/src/Ebus)

add_executable(ebusread ebusread.cpp)
target_link_libraries(ebusread ebus)

add_executable(playground playground.cpp)
target_link_libraries(playground ebus)
