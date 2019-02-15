set(UTP_SRC
  utp_callbacks.cpp
  utp_utils.cpp
  utp_internal.cpp
  utp_api.cpp
  utp_packedsockaddr.cpp
  utp_hash.cpp
)

if(WIN32)
  list(APPEND UTP_SRC libutp_inet_ntop.cpp)
endif(WIN32)

add_library(libutp STATIC ${UTP_SRC})
if(WIN32)
  target_link_libraries(libutp ws2_32)
endif(WIN32)
target_include_directories(libutp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CORE_INCLUDE})
add_log_tag(libutp)
