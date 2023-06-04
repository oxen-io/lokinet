
cmake_minimum_required(VERSION 3.10)

project(udptest LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
add_executable(udptest udptest.cpp)
include_directories(../../include)
target_link_libraries(udptest PUBLIC lokinet)

