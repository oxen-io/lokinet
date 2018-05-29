cmake_minimum_required(VERSION 2.8.10)

set(DEBUG_FLAGS "")

if(ASAN)
  set(DEBUG_FLAGS "${DEBUG_FLAGS} -O1 -g -fsanitize=address -fno-omit-frame-pointer")
endif(ASAN)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c11 -Wall -fPIC ${DEBUG_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -Wall -fPIC ${DEBUG_FLAGS}")

if(NOT GIT_VERSION)
  exec_program("git" ${CMAKE_CURRENT_SOURCE_DIR} ARGS "rev-parse --short HEAD" OUTPUT_VARIABLE GIT_VERSION)
  add_definitions(-DGIT_REV="${GIT_VERSION}")
endif()

if(RELEASE_MOTTO)
  add_definitions(-DLLARP_RELEASE_MOTTO="${RELEASE_MOTTO}")
endif()

set(EXE llarpd)
set(EXE_SRC daemon/main.cpp)

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
  llarp/buffer.cpp
  llarp/config.cpp
  llarp/context.cpp
  llarp/crypto_async.cpp
  llarp/crypto_libsodium.cpp
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
  llarp/router_contact.cpp
  llarp/router.cpp
  llarp/router_identity.c
  llarp/threadpool.cpp
  llarp/time.cpp
  llarp/timer.cpp
)

include_directories(include)

add_library(${STATIC_LIB} STATIC ${LIB_SRC})
add_library(${SHARED_LIB} SHARED ${LIB_SRC})

add_executable(${EXE} ${EXE_SRC})
target_link_libraries(${STATIC_LIB} ${LIBS})
target_link_libraries(${EXE} ${STATIC_LIB})
target_link_libraries(${SHARED_LIB} ${STATIC_LIB} ${LIBS})

add_executable(rcutil daemon/rcutil.cpp)
target_link_libraries(rcutil ${STATIC_LIB} ${LIBS})
