cmake_minimum_required(VERSION 3.10.0)
project(ebus VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14")
set(CMAKE_CXX_STANDARD 14)

add_subdirectory(src/Ebus)

add_subdirectory(tools)

add_subdirectory(test)

add_subdirectory(ebusstack)
