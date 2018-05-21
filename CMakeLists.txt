cmake_minimum_required(VERSION 2.8.10)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c11 -Wall -fPIC")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -Wall -fPIC")

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake ${CMAKE_MODULE_PATH})

#find_package(sodium)

set(EXE llarpd)
set(EXE_SRC daemon/main.cpp)

set(LIBS sodium pthread stdc++fs jemalloc mbedx509 mbedcrypto mbedtls)

set(LIB llarp)

set(LIB_SRC
  llarp/address_info.c
  llarp/buffer.cpp
  llarp/config.cpp
  llarp/crypto_async.cpp
  llarp/crypto_libsodium.cpp
  llarp/ev.cpp
  llarp/exit_info.c
  llarp/exit_route.c
  llarp/iwp_link.cpp
  llarp/link.c
  llarp/link_intro.cpp
  llarp/link_relay_down.cpp
  llarp/link_relay_up.cpp
  llarp/logic.c
  llarp/mem.cpp
  llarp/mem_jemalloc.cpp
  llarp/mem_std.cpp
  llarp/muxer.cpp
  llarp/net.cpp
  llarp/nodedb.cpp
  llarp/router_contact.c
  llarp/router.cpp
  llarp/router_identity.c
  llarp/threadpool.cpp
  llarp/time.cpp
  llarp/timer.cpp
)

include_directories(include)
include_directories(${sodium_INCLUDE_DIR})


add_library(${LIB} STATIC ${LIB_SRC})
add_executable(${EXE} ${EXE_SRC})
target_link_libraries(${EXE} ${LIB} ${LIBS})

add_executable(rcutil daemon/rcutil.cpp)
target_link_libraries(rcutil ${LIB} ${LIBS})
