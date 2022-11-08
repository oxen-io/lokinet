#[[
project(udptest LANGUAGES CXX)
add_executable(udptest udptest.cpp)
include_directories(../../include)
target_link_libraries(udptest PUBLIC lokinet)
]]

add_executable(tcp_listen
    tcp_listen.cpp)
target_link_libraries(tcp_listen PUBLIC lokinet-shared)

add_executable(tcp_connect
    tcp_connect.cpp)
target_link_libraries(tcp_connect PUBLIC lokinet-shared)
