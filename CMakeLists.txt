cmake_minimum_required(VERSION 2.8.10)

set(DEBUG_FLAGS "-O1 -g -fsanitize=address -fno-omit-frame-pointer")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c11 -Wall -fPIC ${DEBUG_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -Wall -fPIC ${DEBUG_FLAGS}")

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake ${CMAKE_MODULE_PATH})

set(EXE llarpd)
set(EXE_SRC daemon/main.cpp)

set(LIBS sodium pthread stdc++fs)


set(LIB llarp)

set(LIB_SRC
  llarp/address_info.cpp
  llarp/buffer.cpp
  llarp/config.cpp
  llarp/crypto_async.cpp
  llarp/crypto_libsodium.cpp
  llarp/ev.cpp
  llarp/exit_info.c
  llarp/exit_route.c
  llarp/iwp_link.cpp
  llarp/logger.cpp
  llarp/link.c
  llarp/link_intro.cpp
  llarp/link_message.cpp
  llarp/logic.c
  llarp/mem.cpp
  llarp/mem_std.cpp
  llarp/net.cpp
  llarp/nodedb.cpp
  llarp/router_contact.cpp
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
