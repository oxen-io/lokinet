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
if (WIN32 AND NOT MINGW)
CHECK_CXX_COMPILER_FLAG("/std:c++17" COMPILER_SUPPORTS_CXX17)
else()
CHECK_CXX_COMPILER_FLAG("-std=c++17" COMPILER_SUPPORTS_CXX17)
endif()

option(HAVE_CXX17_FILESYSTEM "Enable if your C++ compiler and runtime library implements std::[experimental::]filesystem" OFF)

if(COMPILER_SUPPORTS_CXX11 AND NOT HAVE_CXX17_FILESYSTEM)
  add_cxxflags("-std=c++11")
elseif(COMPILER_SUPPORTS_CXX17 AND HAVE_CXX17_FILESYSTEM)
  if (WIN32 AND NOT MINGW)
  add_cxxflags("/std:c++17")
  else()
  add_cxxflags("-std=c++17")
  endif()
  add_definitions(-DUSE_CXX17_FILESYSTEM)
else()
  message(ERROR "The compiler ${CMAKE_CXX_COMPILER} has no C++11 or C++17 support. Please use a different C++ compiler.")
endif()

if(ANDROID)
set(THREAD_LIB "-pthread")
# finally removed pthread dependency for MSC++
elseif(WIN32 AND NOT MINGW)
set(THREAD_LIB)
else()
set(THREAD_LIB pthread)
endif()

if(STATIC_LINK)
  add_cflags("-static -Wl,--whole-archive -lpthread -Wl,--no-whole-archive")
  add_cxxflags("-static -Wl,--whole-archive -lpthread -Wl,--no-whole-archive")
endif()

if(SHADOW)
  set(WITH_STATIC OFF)
else()
  set(WITH_STATIC ON)
endif()

if (UNIX OR MINGW OR APPLE)
add_cflags("-Wall")
add_cxxflags("-Wall")
endif()

if (WIN32 AND NOT MINGW)
set(OPTIMIZE_FLAGS "-Od")
set(DEBUG_FLAGS "-ZI")
else()
set(OPTIMIZE_FLAGS "-O0")
set(DEBUG_FLAGS "-g")
endif()

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

if (WIN32 AND NOT MINGW)
add_cflags("-wd4996 -wd4244 -MP ${OPTIMIZE_FLAGS}")
add_cxxflags("-wd4996 -wd4244 -MP ${OPTIMIZE_FLAGS}")
else()
add_cflags("-Wall -Wno-deprecated-declarations ${OPTIMIZE_FLAGS}")
add_cxxflags("-Wall -Wno-deprecated-declarations ${OPTIMIZE_FLAGS}")
endif()

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

set(LIB lokinet)
set(SHARED_LIB ${LIB})
set(STATIC_LIB ${LIB}-static)

set(TT_ROOT vendor/libtuntap-master)

add_definitions(-D${CMAKE_SYSTEM_NAME})

if (UNIX)
add_definitions(-DUNIX)
endif()

if(UNIX)
    if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
        set(LIBTUNTAP_IMPL ${TT_ROOT}/tuntap-unix-linux.c)
    elseif (${CMAKE_SYSTEM_NAME} MATCHES "OpenBSD")
        set(LIBTUNTAP_IMPL ${TT_ROOT}/tuntap-unix-openbsd.c ${TT_ROOT}/tuntap-unix-bsd.c)
    elseif (${CMAKE_SYSTEM_NAME} MATCHES "NetBSD")
        set(LIBTUNTAP_IMPL ${TT_ROOT}/tuntap-unix-netbsd.c ${TT_ROOT}/tuntap-unix-bsd.c)
    elseif (${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")
        set(LIBTUNTAP_IMPL ${TT_ROOT}/tuntap-unix-freebsd.c ${TT_ROOT}/tuntap-unix-bsd.c)
    elseif (${CMAKE_SYSTEM_NAME} MATCHES "DragonFly")
        set(LIBTUNTAP_IMPL ${TT_ROOT}/tuntap-unix-freebsd.c ${TT_ROOT}/tuntap-unix-bsd.c)
    elseif (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
        set(LIBTUNTAP_IMPL ${TT_ROOT}/tuntap-unix-darwin.c ${TT_ROOT}/tuntap-unix-bsd.c)
    else()
        message(FATAL_ERROR "Your operating system is not supported yet")
    endif()
elseif(WIN32 OR MINGW)
    set(LIBTUNTAP_IMPL ${TT_ROOT}/tuntap-windows.c)
    add_definitions(-DWIN32_LEAN_AND_MEAN)
else()
    message(FATAL_ERROR "What operating system _are_ you building on/for?")
endif(UNIX)

if(TUNTAP)
set(LIBTUNTAP_SRC
  ${TT_ROOT}/tuntap.c
  ${TT_ROOT}/tuntap_log.c
  if (UNIX)
  ${TT_ROOT}/tuntap-unix.c
  endif()
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
# win32 inline procs
  llarp/win32_inet.c
  llarp/win32_intrnl.c
  contrib/msc/getopt.c
  contrib/msc/getopt1.c
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
  llarp/dns.cpp
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

set(ALL_SRC ${CLIENT_SRC} daemon/rcutil.cpp ${EXE_SRC} ${DNS_SRC} ${LIB_PLATFORM_SRC} ${LIB_SRC} ${TEST_SRC} ${CPP_BACKPORT_SRC})

foreach(F ${ALL_SRC})
set_source_files_properties(${F} PROPERTIES COMPILE_FLAGS -DLOG_TAG=\\\"${F}\\\")
endforeach(F)

set(LIB lokinet)
set(SHARED_LIB ${LIB})
set(STATIC_LIB ${LIB}-static)

# TODO: exclude this from includes and expose stuff properly for rcutil
include_directories(llarp)
include_directories(include)
include_directories(vendor/cppbackport-master/lib)
#include_directories(/usr/local/include)
include_directories(${sodium_INCLUDE_DIR})

if (WIN32 AND NOT MINGW)
include_directories(contrib/msc/include)
link_directories(contrib/msc/lib)
endif()

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
    if(NOT HAVE_CXX17_FILESYSTEM)
      add_library(backport-static STATIC ${CPP_BACKPORT_SRC})
    endif(NOT HAVE_CXX17_FILESYSTEM)
    add_library(llarpplatform-static STATIC ${LIB_PLATFORM_SRC})
    target_link_libraries(llarpplatform-static ${THREAD_LIB})
    if(NOT HAVE_CXX17_FILESYSTEM)
      target_link_libraries(${STATIC_LIB} ${LIBS} backport-static llarpplatform-static)
    else()
      target_link_libraries(${STATIC_LIB} ${LIBS} llarpplatform-static)
    endif(NOT HAVE_CXX17_FILESYSTEM)
    if(NOT WITH_SHARED)
      if(NOT HAVE_CXX17_FILESYSTEM)
        target_link_libraries(${EXE} ${STATIC_LINK_LIBS} ${STATIC_LIB} backport-static llarpplatform-static)
        target_link_libraries(${CLIENT_EXE} ${STATIC_LINK_LIBS} ${STATIC_LIB} backport-static llarpplatform-static)
        target_link_libraries(rcutil ${STATIC_LINK_LIBS} ${STATIC_LIB} backport-static llarpplatform-static)
        if (MINGW)
          target_link_libraries(${EXE} ${STATIC_LINK_LIBS} ${STATIC_LIB} backport-static llarpplatform-static ws2_32 stdc++fs iphlpapi)
          target_link_libraries(${CLIENT_EXE} ${STATIC_LINK_LIBS} ${STATIC_LIB} backport-static llarpplatform-static ws2_32 stdc++fs iphlpapi)
          target_link_libraries(rcutil ${STATIC_LINK_LIBS} ${STATIC_LIB} backport-static llarpplatform-static ws2_32 stdc++fs iphlpapi)
        elseif(WIN32)
          target_link_libraries(${EXE} ${STATIC_LINK_LIBS} ${STATIC_LIB} backport-static llarpplatform-static ws2_32 iphlpapi)
          target_link_libraries(${CLIENT_EXE} ${STATIC_LINK_LIBS} ${STATIC_LIB} backport-static llarpplatform-static ws2_32 iphlpapi)
          target_link_libraries(rcutil ${STATIC_LINK_LIBS} ${STATIC_LIB} backport-static llarpplatform-static ws2_32 iphlpapi)
        endif(MINGW)
      else()
        target_link_libraries(${EXE} ${STATIC_LINK_LIBS} ${STATIC_LIB} llarpplatform-static)
        target_link_libraries(${CLIENT_EXE} ${STATIC_LINK_LIBS} ${STATIC_LIB} llarpplatform-static)
        target_link_libraries(rcutil ${STATIC_LINK_LIBS} ${STATIC_LIB} llarpplatform-static)
        if (MINGW)
          target_link_libraries(${EXE} ${STATIC_LINK_LIBS} ${STATIC_LIB} llarpplatform-static ws2_32 stdc++fs iphlpapi)
          target_link_libraries(${CLIENT_EXE} ${STATIC_LINK_LIBS} ${STATIC_LIB} llarpplatform-static ws2_32 stdc++fs iphlpapi)
          target_link_libraries(rcutil ${STATIC_LINK_LIBS} ${STATIC_LIB} llarpplatform-static ws2_32 stdc++fs iphlpapi)
        elseif(WIN32)
          target_link_libraries(${EXE} ${STATIC_LINK_LIBS} ${STATIC_LIB} llarpplatform-static ws2_32 iphlpapi)
          target_link_libraries(${CLIENT_EXE} ${STATIC_LINK_LIBS} ${STATIC_LIB} llarpplatform-static ws2_32 iphlpapi)
          target_link_libraries(rcutil ${STATIC_LINK_LIBS} ${STATIC_LIB} llarpplatform-static ws2_32 iphlpapi)
        endif(MINGW)
      endif(NOT HAVE_CXX17_FILESYSTEM)
      if (MINGW)
        target_link_libraries(dns ${STATIC_LIB} llarpplatform-static ${THREAD_LIB} ws2_32 stdc++fs iphlpapi)
      elseif(WIN32)
        target_link_libraries(dns ${STATIC_LIB} llarpplatform-static ${THREAD_LIB} ws2_32 iphlpapi)
      endif(MINGW)
      target_link_libraries(dns ${STATIC_LIB} llarpplatform-static ${THREAD_LIB})
    endif(NOT WITH_SHARED)
  endif(WITH_STATIC)

  if(WITH_SHARED)
    if(HAVE_CXX17_FILESYSTEM)
      set(LIB_SRC ${LIB_SRC} ${CPP_BACKPORT_SRC})
    endif(HAVE_CXX17_FILESYSTEM)
    add_library(${SHARED_LIB} SHARED ${LIB_SRC} ${LIB_PLATFORM_SRC})
    if (MINGW)
      set(${LIBS} ${LIBS} ws2_32 stdc++fs iphlpapi)
    elseif(WIN32)
      set(${LIBS} ${LIBS} ws2_32 iphlpapi)
    endif(MINGW)
    target_link_libraries(${SHARED_LIB} ${LIBS} ${THREAD_LIB})
    target_link_libraries(${EXE} ${SHARED_LIB})
    target_link_libraries(rcutil ${SHARED_LIB})
    target_link_libraries(dns ${SHARED_LIB} ${THREAD_LIB})
  endif(WITH_SHARED)
endif(SHADOW)
