add_library(${ABYSS_LIB} "${CMAKE_CURRENT_SOURCE_DIR}/src/md5.cpp"
                         "${CMAKE_CURRENT_SOURCE_DIR}/src/http.cpp"
                         "${CMAKE_CURRENT_SOURCE_DIR}/src/client.cpp"
                         "${CMAKE_CURRENT_SOURCE_DIR}/src/server.cpp")

target_include_directories(${ABYSS_LIB} PUBLIC include)
target_link_libraries(${ABYSS_LIB} PUBLIC ${PLATFORM_LIB})

if(NOT WIN32 AND NOT CMAKE_SYSTEM_NAME STREQUAL iOS)
  target_link_libraries(${ABYSS_LIB} PUBLIC ${LIBUV_LIBRARY})
  add_executable(${ABYSS_EXE} main.cpp)
  target_link_libraries(${ABYSS_EXE} PUBLIC ${ABYSS_LIB} Threads::Threads ${LIBS})
elseif(MSVC_VERSION)
  add_executable(${ABYSS_EXE} main.cpp)
  target_link_libraries(${ABYSS_EXE} PUBLIC ${ABYSS_LIB} ${STATIC_LIB} ws2_32)
elseif(NOT CMAKE_SYSTEM_NAME STREQUAL iOS)
  add_executable(${ABYSS_EXE} main.cpp llarp/win32/abyss.rc)
  target_link_libraries(${ABYSS_EXE} PUBLIC ${ABYSS_LIB} ${STATIC_LIB} ws2_32)
endif()

# for freebsd
if(${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")
  target_link_directories(${ABYSS_EXE} PRIVATE /usr/local/lib)
  target_include_directories(${ABYSS_LIB} SYSTEM PUBLIC /usr/local/include)
endif(${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")

add_log_tag(${ABYSS_EXE})
add_log_tag(${ABYSS_LIB})
