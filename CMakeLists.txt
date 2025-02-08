cmake_minimum_required(VERSION 3.5.0)
project(ebus VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14")

add_subdirectory(src)

add_subdirectory(tools)

add_subdirectory(ebusstack)
