cmake_minimum_required(VERSION 2.8.10)

macro(add_cflags)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${ARGN}")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${ARGN}")
endmacro(add_cflags)
  
set(WITH_SHARED OFF)
set(DEBUG_FLAGS "-g")
set(OPTIMIZE_FLAGS "-Os")
if(ASAN)
  set(DEBUG_FLAGS "${DEBUG_FLAGS} -fsanitize=address -fno-omit-frame-pointer")
  set(OPTIMIZE_FLAGS "-O0")
endif(ASAN)

if(SHADOW)
  add_definitions(-DTESTNET=true)
  add_cflags("-fPIC")
endif()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c11 -Wall ${DEBUG_FLAGS} ${OPTIMIZE_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -Wall ${DEBUG_FLAGS} ${OPTIMIZE_FLAGS}")

if(NOT GIT_VERSION)
  exec_program("git" ${CMAKE_CURRENT_SOURCE_DIR} ARGS "rev-parse --short HEAD" OUTPUT_VARIABLE GIT_VERSION)
  add_definitions(-DGIT_REV="${GIT_VERSION}")
endif()

if(RELEASE_MOTTO)
  add_definitions(-DLLARP_RELEASE_MOTTO="${RELEASE_MOTTO}")
endif()

set(EXE llarpd)
set(EXE_SRC daemon/main.c)

if(SODIUM_INCLUDE_DIR)
  include_directories(${SODIUM_INCLUDE_DIR})
endif()

if(SODIUM_LIBRARIES)
  set(SODIUM_LIB ${SODIUM_LIBRARIES})
else()
  set(SODIUM_LIB sodium)
endif()

set(LIBS ${SODIUM_LIB} pthread stdc++fs)

set(LIB llarp)
set(SHARED_LIB ${LIB})
set(STATIC_LIB ${LIB}-static)

set(LIB_SRC
  llarp/address_info.cpp
  llarp/bencode.c
  llarp/buffer.cpp
  llarp/config.cpp
  llarp/context.cpp
  llarp/crypto_async.cpp
  llarp/crypto_libsodium.cpp
  llarp/dht.cpp
  llarp/ev.cpp
  llarp/exit_info.cpp
  llarp/exit_route.c
  llarp/iwp_link.cpp
  llarp/logger.cpp
  llarp/link.c
  llarp/link_intro.cpp
  llarp/link_message.cpp
  llarp/logic.cpp
  llarp/mem.cpp
  llarp/net.cpp
  llarp/nodedb.cpp
  llarp/relay_commit.cpp
  llarp/router_contact.cpp
  llarp/router.cpp
  llarp/router_identity.c
  llarp/threadpool.cpp
  llarp/time.cpp
  llarp/timer.cpp
)

include_directories(include)

add_library(${STATIC_LIB} STATIC ${LIB_SRC})
if(WITH_SHARED)
  add_library(${SHARED_LIB} SHARED ${LIB_SRC})
  target_link_libraries(${SHARED_LIB} ${LIBS})
endif()

add_executable(${EXE} ${EXE_SRC})
target_link_libraries(${STATIC_LIB} ${LIBS})
target_link_libraries(${EXE} ${STATIC_LIB})

add_executable(rcutil daemon/rcutil.cpp)
target_link_libraries(rcutil ${STATIC_LIB} ${LIBS})


if(SHADOW)
  if("${SHADOW_ROOT}" STREQUAL "")
    set(SHADOW_ROOT "$ENV{HOME}/.shadow")
  endif("${SHADOW_ROOT}" STREQUAL "")
  if(EXISTS "${SHADOW_ROOT}")
    message(STATUS "SHADOW_ROOT = ${SHADOW_ROOT}")
  else()
    message(FATAL_ERROR "SHADOW_ROOT path does not exist: '${SHADOW_ROOT}'")
  endif()
  set(CMAKE_MODULE_PATH "${SHADOW_ROOT}/share/cmake/Modules")
  include_directories(${CMAKE_MODULE_PATH})
  include_directories(${SHADOW_ROOT}/include)
  include(ShadowTools)
  add_cflags("-fno-inline -fno-strict-aliasing")
  add_shadow_plugin(shadow-llarpd ${EXE_SRC})
  target_link_libraries(shadow-llarpd ${STATIC_LIB})
  install(TARGETS shadow-llarpd DESTINATION plugins)
endif()
