cmake_minimum_required(VERSION 2.8.10)

set(PROJECT_NAME llarpd)
project(${PROJECT_NAME})

macro(add_cflags)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${ARGN}")
endmacro(add_cflags)

macro(add_cxxflags)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${ARGN}")
endmacro(add_cxxflags)

include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG("-std=c++11" COMPILER_SUPPORTS_CXX11)
CHECK_CXX_COMPILER_FLAG("-std=c++17" COMPILER_SUPPORTS_CXX17)
if(COMPILER_SUPPORTS_CXX11 AND NOT COMPILER_SUPPORTS_CXX17 OR ${CMAKE_SYSTEM_NAME} MATCHES "OpenBSD")
  add_cxxflags("-std=c++11")
elseif(COMPILER_SUPPORTS_CXX17)
  add_cxxflags("-std=c++17")
else()
  message(ERROR "The compiler ${CMAKE_CXX_COMPILER} has no C++11 or C++17 support. Please use a different C++ compiler.")
endif()

# attempting to fix linux gcc 7.2 which COMPILER_SUPPORTS_CXX17 but doesn't have filesystem.h
include(CheckIncludeFileCXX)
CHECK_INCLUDE_FILE_CXX(filesystem.h HAVE_STD_FS)

set(THREAD_LIB "-pthread")

if(STATIC_LINK)
  set(STATIC_LINK_LIBS c)
  add_cflags("-static -Wl,--whole-archive -lpthread -Wl,--no-whole-archive")
  add_cxxflags("-static -Wl,--whole-archive -lpthread -Wl,--no-whole-archive")
endif()

if(SHADOW)
  set(WITH_STATIC OFF)
else()
  set(WITH_STATIC ON)
endif()

add_cflags("-Wall")
add_cxxflags("-Wall")

set(DEBUG_FLAGS "-g")
set(OPTIMIZE_FLAGS "-O0")

if(ASAN)
  set(DEBUG_FLAGS "${DEBUG_FLAGS} -fsanitize=address -fno-omit-frame-pointer")
  set(OPTIMIZE_FLAGS "-O0")
endif(ASAN)

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
  include(ShadowTools)
  add_cxxflags("-fno-inline -fno-strict-aliasing")
  add_cflags("-fno-inline -fno-strict-aliasing")

  add_definitions(-DTESTNET=true)
  add_definitions(-DSHADOW_TESTNET)
  include_directories(${SHADOW_ROOT}/include)
endif()

add_cflags("-Wall -Wno-deprecated-declarations ${OPTIMIZE_FLAGS}")
add_cxxflags("-Wall -Wno-deprecated-declarations ${OPTIMIZE_FLAGS}")

if(CMAKE_BUILD_TYPE MATCHES "[Dd][Ee][Bb][Uu][Gg]")
  add_cflags("${DEBUG_FLAGS}")
  add_cxxflags("${DEBUG_FLAGS}")
endif()

if(SHADOW)
  add_cflags("-fPIC")
  add_cxxflags("-fPIC")
endif()

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
  find_library (
          SODIUM_LIB
          NAMES sodium libsodium
          HINTS "/usr/local/lib"
  )
endif()

set(LIBS ${SODIUM_LIB} ${THREAD_LIB})

set(LIB llarp)
set(SHARED_LIB ${LIB})
set(STATIC_LIB ${LIB}-static)

set(TT_ROOT vendor/libtuntap-master)

add_definitions(-D${CMAKE_SYSTEM_NAME})
add_definitions(-DUNIX)
if(UNIX)
    if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
        set(LIBTUNTAP_IMPL ${TT_ROOT}/tuntap-unix-linux.c)
    elseif (${CMAKE_SYSTEM_NAME} MATCHES "OpenBSD")
        set(LIBTUNTAP_IMPL ${TT_ROOT}/tuntap-unix-openbsd.c ${TT_ROOT}/tuntap-unix-bsd.c)
    elseif (${CMAKE_SYSTEM_NAME} MATCHES "NetBSD")
        set(LIBTUNTAP_IMPL ${TT_ROOT}/tuntap-unix-netbsd.c ${TT_ROOT}/tuntap-unix-bsd.c)
    elseif (${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")
        set(LIBTUNTAP_IMPL ${TT_ROOT}/tuntap-unix-freebsd.c ${TT_ROOT}/tuntap-unix-bsd.c)
    elseif (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
        set(LIBTUNTAP_IMPL ${TT_ROOT}/tuntap-unix-darwin.c ${TT_ROOT}/tuntap-unix-bsd.c)
    elseif (${CMAKE_SYSTEM_NAME} MATCHES "DragonFly")
        set(LIBTUNTAP_IMPL ${TT_ROOT}/tuntap-unix-freebsd.c ${TT_ROOT}/tuntap-unix-bsd.c)
    else()
        message(FATAL_ERROR "Your operating system is not supported yet")
    endif()
endif(UNIX)

if(TUNTAP)
set(LIBTUNTAP_SRC
  ${TT_ROOT}/tuntap.c
  ${TT_ROOT}/tuntap_log.c
  ${TT_ROOT}/tuntap-unix.c
  ${LIBTUNTAP_IMPL})
else()
set(LIBTUNTAP_SRC "")
endif()

#if(BACKPORT)
#else()
#set(CPP_BACKPORT_SRC "")
#endif()

set(CPP_BACKPORT_SRC
  vendor/cppbackport-master/lib/fs/rename.cpp
  vendor/cppbackport-master/lib/fs/filestatus.cpp
  vendor/cppbackport-master/lib/fs/filetype.cpp
  vendor/cppbackport-master/lib/fs/cleanpath.cpp
  vendor/cppbackport-master/lib/fs/perms.cpp
  vendor/cppbackport-master/lib/fs/equivalent.cpp
  vendor/cppbackport-master/lib/fs/current_path.cpp
  vendor/cppbackport-master/lib/fs/basename.cpp
  vendor/cppbackport-master/lib/fs/tempdir.cpp
  vendor/cppbackport-master/lib/fs/create_directory.cpp
  vendor/cppbackport-master/lib/fs/path.cpp
  vendor/cppbackport-master/lib/fs/remove.cpp
  vendor/cppbackport-master/lib/fs/diriter.cpp
  vendor/cppbackport-master/lib/fs/copyfile.cpp
  vendor/cppbackport-master/lib/fs/absolute.cpp
  vendor/cppbackport-master/lib/fs/direntry.cpp
)

if(ANDROID)
  add_definitions(-DANDROID)
  set(ANDROID_PLATFORM_SRC llarp/android/ifaddrs.c)
endif()

set(LIB_PLATFORM_SRC
# for outpug
  llarp/logger.cpp
# needed for threading
  llarp/logic.cpp
# for zero (for net.hpp stuff for llarp::addr stuff)
  llarp/mem.cpp
# for networking
  llarp/ev.cpp
# for timer
  llarp/time.cpp
# for logic
  llarp/timer.cpp
# for threading
  llarp/threadpool.cpp
# for android shim
  ${ANDROID_PLATFORM_SRC}
)



set(LIB_SRC
  llarp/address_info.cpp
  llarp/bencode.cpp
  llarp/buffer.cpp
  llarp/config.cpp
  llarp/context.cpp
  llarp/crypto_async.cpp
  llarp/crypto_libsodium.cpp
  llarp/dht.cpp
  llarp/dnsc.cpp
  llarp/dnsd.cpp
  llarp/encode.cpp
  llarp/encrypted_frame.cpp
  llarp/exit_info.cpp
  llarp/exit_route.cpp
  llarp/link_intro.cpp
  llarp/link_message.cpp
  llarp/net.cpp
  llarp/nodedb.cpp
  llarp/path.cpp
  llarp/pathbuilder.cpp
  llarp/pathset.cpp
  llarp/proofofwork.cpp
  llarp/relay_ack.cpp
  llarp/relay_commit.cpp
  llarp/relay_up_down.cpp
  llarp/router_contact.cpp
  llarp/router.cpp
  llarp/service.cpp
  llarp/transit_hop.cpp
  llarp/testnet.c
  llarp/dht/context.cpp
  llarp/dht/decode.cpp
  llarp/dht/dht_immediate.cpp
  llarp/dht/find_intro.cpp
  llarp/dht/find_router.cpp
  llarp/dht/got_intro.cpp
  llarp/dht/got_router.cpp
  llarp/dht/search_job.cpp
  llarp/dht/publish_intro.cpp
  llarp/iwp/frame_header.cpp
  llarp/iwp/frame_state.cpp
  llarp/iwp/session.cpp
  llarp/iwp/server.cpp
  llarp/iwp/transit_message.cpp
  llarp/iwp/xmit.cpp
  llarp/link/encoder.cpp
  llarp/routing/dht_message.cpp
  llarp/routing/message_parser.cpp
  llarp/routing/path_confirm.cpp
  llarp/routing/path_latency.cpp
  llarp/routing/path_transfer.cpp
  llarp/service/address.cpp
  llarp/service/context.cpp
  llarp/service/endpoint.cpp
  llarp/service/lookup.cpp
  llarp/service/protocol.cpp
  llarp/service/tag.cpp
  llarp/service/info.cpp
  ${LIBTUNTAP_SRC}
)

set(DNS_SRC
  llarp/dns.cpp
  llarp/dnsc.cpp
  llarp/dnsd.cpp
  llarp/net.cpp
  daemon/dns.cpp
)

set(TEST_SRC
  test/main.cpp
  test/base32_unittest.cpp
  test/dht_unittest.cpp
  test/encrypted_frame_unittest.cpp
  test/hiddenservice_unittest.cpp
)
set(TEST_EXE testAll)
set(GTEST_DIR test/gtest)

set(CLIENT_EXE llarpc)

set(CLIENT_SRC
  client/main.cpp
)

#set(LIB llarp)
#set(SHARED_LIB ${LIB})
#set(STATIC_LIB ${LIB}-static)

# TODO: exclude this from includes and expose stuff properly for rcutil
include_directories(llarp)

include_directories(include)
include_directories(vendor/cppbackport-master/lib)
#include_directories(/usr/local/include)
include_directories(${sodium_INCLUDE_DIR})

if(SHADOW)
  add_shadow_plugin(shadow-plugin-${SHARED_LIB} ${EXE_SRC} ${LIB_SRC})
  target_link_libraries(shadow-plugin-${SHARED_LIB} ${LIBS})
  install(TARGETS shadow-plugin-${SHARED_LIB} DESTINATION plugins)
else()

  add_executable(rcutil daemon/rcutil.cpp)
  add_executable(${EXE} ${EXE_SRC})
  add_executable(${CLIENT_EXE} ${CLIENT_SRC})
  add_executable(dns ${DNS_SRC})

  if(WITH_TESTS)
    enable_testing()
    add_subdirectory(${GTEST_DIR})
    include_directories(${GTEST_DIR}/include ${GTEST_DIR})
    add_executable(${TEST_EXE} ${TEST_SRC})
    add_test(runAllTests ${TEST_EXE})
    target_link_libraries(${TEST_EXE} ${STATIC_LINK_LIBS} gtest_main ${STATIC_LIB})
  endif()

  if(WITH_STATIC)
    add_library(${STATIC_LIB} STATIC ${LIB_SRC})
    if(COMPILER_SUPPORTS_CXX11 AND NOT COMPILER_SUPPORTS_CXX17 OR ${CMAKE_SYSTEM_NAME} MATCHES "OpenBSD" OR NOT HAVE_STD_FS)
      add_library(backport-static STATIC ${CPP_BACKPORT_SRC})
    endif(COMPILER_SUPPORTS_CXX11 AND NOT COMPILER_SUPPORTS_CXX17 OR ${CMAKE_SYSTEM_NAME} MATCHES "OpenBSD" OR NOT HAVE_STD_FS)
    add_library(llarpplatform-static STATIC ${LIB_PLATFORM_SRC})
    target_link_libraries(llarpplatform-static ${THREAD_LIB})
    if(COMPILER_SUPPORTS_CXX11 AND NOT COMPILER_SUPPORTS_CXX17 OR ${CMAKE_SYSTEM_NAME} MATCHES "OpenBSD" OR NOT HAVE_STD_FS)
      target_link_libraries(${STATIC_LIB} ${LIBS} backport-static llarpplatform-static)
    else()
      target_link_libraries(${STATIC_LIB} ${LIBS} llarpplatform-static)
    endif(COMPILER_SUPPORTS_CXX11 AND NOT COMPILER_SUPPORTS_CXX17 OR ${CMAKE_SYSTEM_NAME} MATCHES "OpenBSD" OR NOT HAVE_STD_FS)
    if(NOT WITH_SHARED)
      if(COMPILER_SUPPORTS_CXX11 AND NOT COMPILER_SUPPORTS_CXX17 OR ${CMAKE_SYSTEM_NAME} MATCHES "OpenBSD" OR NOT HAVE_STD_FS)
        target_link_libraries(${EXE} ${STATIC_LINK_LIBS} ${STATIC_LIB} backport-static llarpplatform-static)
        target_link_libraries(${CLIENT_EXE} ${STATIC_LINK_LIBS} ${STATIC_LIB} backport-static llarpplatform-static)
        target_link_libraries(rcutil ${STATIC_LINK_LIBS} ${STATIC_LIB} backport-static llarpplatform-static)
        if (MINGW)
          target_link_libraries(${EXE} ${STATIC_LINK_LIBS} ${STATIC_LIB} backport-static llarpplatform-static ws2_32 stdc++fs)
          target_link_libraries(${CLIENT_EXE} ${STATIC_LINK_LIBS} ${STATIC_LIB} backport-static llarpplatform-static ws2_32 stdc++fs)
          target_link_libraries(rcutil ${STATIC_LINK_LIBS} ${STATIC_LIB} backport-static llarpplatform-static ws2_32 stdc++fs)
        elseif(WIN32)
          target_link_libraries(${EXE} ${STATIC_LINK_LIBS} ${STATIC_LIB} backport-static llarpplatform-static ws2_32)
          target_link_libraries(${CLIENT_EXE} ${STATIC_LINK_LIBS} ${STATIC_LIB} backport-static llarpplatform-static ws2_32)
          target_link_libraries(rcutil ${STATIC_LINK_LIBS} ${STATIC_LIB} backport-static llarpplatform-static ws2_32)
        endif(MINGW)
      else()
        target_link_libraries(${EXE} ${STATIC_LINK_LIBS} ${STATIC_LIB} llarpplatform-static)
        target_link_libraries(${CLIENT_EXE} ${STATIC_LINK_LIBS} ${STATIC_LIB} llarpplatform-static)
        target_link_libraries(rcutil ${STATIC_LINK_LIBS} ${STATIC_LIB} llarpplatform-static)
        if (MINGW)
          target_link_libraries(${EXE} ${STATIC_LINK_LIBS} ${STATIC_LIB} llarpplatform-static ws2_32 stdc++fs)
          target_link_libraries(${CLIENT_EXE} ${STATIC_LINK_LIBS} ${STATIC_LIB} llarpplatform-static ws2_32 stdc++fs)
          target_link_libraries(rcutil ${STATIC_LINK_LIBS} ${STATIC_LIB} llarpplatform-static ws2_32 stdc++fs)
        elseif(WIN32)
          target_link_libraries(${EXE} ${STATIC_LINK_LIBS} ${STATIC_LIB} llarpplatform-static ws2_32)
          target_link_libraries(${CLIENT_EXE} ${STATIC_LINK_LIBS} ${STATIC_LIB} llarpplatform-static ws2_32)
          target_link_libraries(rcutil ${STATIC_LINK_LIBS} ${STATIC_LIB} llarpplatform-static ws2_32)
        endif(MINGW)
      endif(COMPILER_SUPPORTS_CXX11 AND NOT COMPILER_SUPPORTS_CXX17 OR ${CMAKE_SYSTEM_NAME} MATCHES "OpenBSD" OR NOT HAVE_STD_FS)
      if (MINGW)
        target_link_libraries(dns ${STATIC_LIB} llarpplatform-static pthread ws2_32 stdc++fs)
      elseif(WIN32)
        target_link_libraries(dns ${STATIC_LIB} llarpplatform-static pthread ws2_32)
      endif(MINGW)
      target_link_libraries(dns ${STATIC_LIB} llarpplatform-static pthread)
    endif(NOT WITH_SHARED)
  endif(WITH_STATIC)

  if(WITH_SHARED)
    add_library(${SHARED_LIB} SHARED ${LIB_SRC})
    if(COMPILER_SUPPORTS_CXX11 AND NOT COMPILER_SUPPORTS_CXX17 OR ${CMAKE_SYSTEM_NAME} MATCHES "OpenBSD" OR NOT HAVE_STD_FS)
      add_library(backport SHARED ${CPP_BACKPORT_SRC})
    endif(COMPILER_SUPPORTS_CXX11 AND NOT COMPILER_SUPPORTS_CXX17 OR ${CMAKE_SYSTEM_NAME} MATCHES "OpenBSD" OR NOT HAVE_STD_FS)
    add_library(llarpplatform SHARED ${LIB_PLATFORM_SRC})
    if(COMPILER_SUPPORTS_CXX11 AND NOT COMPILER_SUPPORTS_CXX17 OR ${CMAKE_SYSTEM_NAME} MATCHES "OpenBSD" OR NOT HAVE_STD_FS)
      target_link_libraries(${SHARED_LIB} ${LIBS} backport llarpplatform)
      if (MINGW)
        target_link_libraries(${SHARED_LIB} ${LIBS} backport llarpplatform ws2_32 stdc++fs)
      elseif(WIN32)
        target_link_libraries(${SHARED_LIB} ${LIBS} backport llarpplatform ws2_32)
      endif(MINGW)
    else()
      target_link_libraries(${SHARED_LIB} ${LIBS} llarpplatform)
      if (MINGW)
        target_link_libraries(${SHARED_LIB} ${LIBS} llarpplatform ws2_32 stdc++fs)
      elseif(WIN32)
        target_link_libraries(${SHARED_LIB} ${LIBS} llarpplatform ws2_32)
      endif(MINGW)
    endif(COMPILER_SUPPORTS_CXX11 AND NOT COMPILER_SUPPORTS_CXX17 OR ${CMAKE_SYSTEM_NAME} MATCHES "OpenBSD" OR NOT HAVE_STD_FS)
    if(NOT WITH_STATIC)
      if(COMPILER_SUPPORTS_CXX11 AND NOT COMPILER_SUPPORTS_CXX17 OR ${CMAKE_SYSTEM_NAME} MATCHES "OpenBSD" OR NOT HAVE_STD_FS)
        target_link_libraries(${EXE} ${SHARED_LIB} backport llarpplatform)
        target_link_libraries(${CLIENT_EXE} ${SHARED_LIB} backport llarpplatform)
        target_link_libraries(rcutil ${SHARED_LIB} backport llarpplatform)
      else()
        target_link_libraries(${EXE} ${SHARED_LIB} llarpplatform)
        target_link_libraries(${CLIENT_EXE} ${SHARED_LIB} llarpplatform)
        target_link_libraries(rcutil ${SHARED_LIB} llarpplatform)
      endif(COMPILER_SUPPORTS_CXX11 AND NOT COMPILER_SUPPORTS_CXX17 OR ${CMAKE_SYSTEM_NAME} MATCHES "OpenBSD" OR NOT HAVE_STD_FS)
    target_link_libraries(dns ${SHARED_LIB} llarpplatform pthread)
    endif(NOT WITH_STATIC)
  endif(WITH_SHARED)
endif(SHADOW)
