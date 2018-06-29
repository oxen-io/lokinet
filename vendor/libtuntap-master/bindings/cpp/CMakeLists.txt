# libtuntap C++ binding CMakeLists.txt
# ====================================

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/bindings/cpp
)

add_library(tuntap++ SHARED
    ${CMAKE_CURRENT_SOURCE_DIR}/bindings/cpp/tuntap++.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/bindings/cpp/tuntap++.hh
)
set_target_properties(tuntap++ PROPERTIES VERSION 2.1)

add_library(tuntap++-static STATIC
        ${CMAKE_CURRENT_SOURCE_DIR}/bindings/cpp/tuntap++.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/bindings/cpp/tuntap++.hh
)
set_target_properties(tuntap++-static PROPERTIES OUTPUT_NAME "tuntap++")

target_link_libraries(tuntap++ tuntap)

target_link_libraries(tuntap++-static tuntap)

target_include_directories(tuntap++ INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/bindings/cpp)
