# Basic definitions
set(LIB lokinet)
set(SHARED_LIB ${LIB}-shared)
set(STATIC_LIB ${LIB}-static)
set(CRYPTOGRAPHY_LIB ${LIB}-cryptography)
set(UTIL_LIB ${LIB}-util)
set(PLATFORM_LIB ${LIB}-platform)
set(ANDROID_LIB ${LIB}android)
set(ABYSS libabyss)
set(ABYSS_LIB abyss)
set(DOCS_SRC "")
get_filename_component(TT_ROOT "${CMAKE_CURRENT_LIST_DIR}/../vendor/libtuntap-master" ABSOLUTE)
add_definitions(-D${CMAKE_SYSTEM_NAME})

get_filename_component(CORE_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/include" ABSOLUTE)
get_filename_component(ABYSS_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/${ABYSS}/include" ABSOLUTE)

set(LIBTUNTAP_SRC
  ${TT_ROOT}/tuntap.cpp
  ${TT_ROOT}/tuntap_log.cpp)
