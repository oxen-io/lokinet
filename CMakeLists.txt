cmake_minimum_required(VERSION 2.8.10)

set(PROJECT_NAME lokinet)
project(${PROJECT_NAME} C CXX ASM)

option(USE_LIBABYSS "enable libabyss" OFF)

macro(add_cflags)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${ARGN}")
endmacro(add_cflags)

macro(add_cxxflags)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${ARGN}")
endmacro(add_cxxflags)

include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG("-std=c++11" COMPILER_SUPPORTS_CXX11)
if(COMPILER_SUPPORTS_CXX11)
  add_cxxflags("-std=c++11")
else()
  message(ERROR "The compiler ${CMAKE_CXX_COMPILER} has no C++11 support. Please use a different C++ compiler.")
endif()

add_cxxflags("-fpermissive")

if (WOW64_CROSS_COMPILE OR WIN64_CROSS_COMPILE)
if (USING_CLANG)
add_cxxflags("-Wno-unused-command-line-argument -Wno-c++11-narrowing")
add_cflags("-Wno-unused-command-line-argument")
# because clang is insane enough to inline whole sections of the C++ library!
# May have been fixed in llvm-7.
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--allow-multiple-definition --rtlib=libgcc")
endif(USING_CLANG)
endif()

if(DEBIAN)
  add_definitions(-DDEBIAN)
endif()

if(ANDROID)
set(THREAD_LIB "-pthread")
else()
set(THREAD_LIB pthread)
endif()

if(STATIC_LINK)
  add_cflags("-static -Wl,--whole-archive -lpthread -Wl,--no-whole-archive")
  add_cxxflags("-static -Wl,--whole-archive -lpthread -Wl,--no-whole-archive")
endif()

if(DNS_PORT)
  add_definitions(-DDNS_PORT=${DNS_PORT})
endif()

if(SHADOW)
  set(WITH_STATIC OFF)
else()
  set(WITH_STATIC ON)
endif()

if(TESTNET)
  add_definitions(-DTESTNET=1)
endif()

add_cflags("-Wall")
add_cxxflags("-Wall")

set(OPTIMIZE_FLAGS "-O3")
set(DEBUG_FLAGS "-O0 -g3")

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

if(CMAKE_BUILD_TYPE MATCHES "[Dd][Ee][Bb][Uu][Gg]")
  set(OPTIMIZE_FLAGS "")
  add_cflags("${DEBUG_FLAGS}")
  add_cxxflags("${DEBUG_FLAGS}")
endif()

set(CRYPTO_FLAGS "-march=native")
set(CMAKE_ASM_FLAGS "-march=native")

add_cflags("-Wall -Wno-deprecated-declarations ${OPTIMIZE_FLAGS} ${CRYPTO_FLAGS}")
add_cxxflags("-Wall -Wno-deprecated-declarations ${OPTIMIZE_FLAGS} ${CRYPTO_FLAGS}")

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

set(EXE lokinet)
set(EXE_SRC daemon/main.cpp)

# HeapAlloc(2) on Windows was significantly revamped in 2009
# but the old algorithm isn't too bad either
# this is _the_ system allocator on BSD UNIX
# openbsd replaced it with a secure/randomised malloc not too
# long ago
if(JEMALLOC)
  set(MALLOC_LIB jemalloc)
endif()

#set(FS_LIB stdc++fs)
set(LIBS ${THREAD_LIB} ${MALLOC_LIB} ${FS_LIB})

set(LIB lokinet)
set(SHARED_LIB ${LIB})
set(STATIC_LIB ${LIB}-static)
set(CRYPTOGRAPHY_LIB ${LIB}-cryptography)
set(PLATFORM_LIB ${LIB}-platform)
set(ANDROID_LIB ${LIB}android)

set(TT_ROOT vendor/libtuntap-master)

add_definitions(-D${CMAKE_SYSTEM_NAME})

if (UNIX)
  add_definitions(-DUNIX)
  add_definitions(-DPOSIX)
endif()

if(UNIX)
    if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
        set(LIBTUNTAP_IMPL ${TT_ROOT}/tuntap-unix-linux.c)
    elseif(${CMAKE_SYSTEM_NAME} MATCHES "Android")
        set(LIBTUNTAP_IMPL ${TT_ROOT}/tuntap-unix-linux.c)
    elseif (${CMAKE_SYSTEM_NAME} MATCHES "OpenBSD")
        set(LIBTUNTAP_IMPL ${TT_ROOT}/tuntap-unix-openbsd.c ${TT_ROOT}/tuntap-unix-bsd.c)
    elseif (${CMAKE_SYSTEM_NAME} MATCHES "NetBSD")
        set(LIBTUNTAP_IMPL ${TT_ROOT}/tuntap-unix-netbsd.c ${TT_ROOT}/tuntap-unix-bsd.c)
    elseif (${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD" OR ${CMAKE_SYSTEM_NAME} MATCHES "DragonFly")
        set(LIBTUNTAP_IMPL ${TT_ROOT}/tuntap-unix-freebsd.c ${TT_ROOT}/tuntap-unix-bsd.c)
    elseif (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
        set(LIBTUNTAP_IMPL ${TT_ROOT}/tuntap-unix-darwin.c ${TT_ROOT}/tuntap-unix-bsd.c)
    elseif (${CMAKE_SYSTEM_NAME} MATCHES "SunOS")
        set(LIBTUNTAP_IMPL ${TT_ROOT}/tuntap-unix-sunos.c)
    else()
        message(FATAL_ERROR "Your operating system is not supported yet")
    endif()
elseif(WIN32)
    set(LIBTUNTAP_IMPL ${TT_ROOT}/tuntap-windows.c)
    add_definitions(-DWIN32_LEAN_AND_MEAN)
    add_definitions(-DWIN32)
else()
    message(FATAL_ERROR "What operating system _are_ you building on/for?")
endif(UNIX)

set(LIBTUNTAP_SRC_BASE
  ${TT_ROOT}/tuntap.cpp
  ${TT_ROOT}/tuntap_log.cpp
  ${LIBTUNTAP_IMPL})

if (UNIX)
set(LIBTUNTAP_SRC
    ${TT_ROOT}/tuntap-unix.c
    ${LIBTUNTAP_SRC_BASE})
else()
set(LIBTUNTAP_SRC ${LIBTUNTAP_SRC_BASE})
endif(UNIX)

if(ANDROID)
  add_definitions(-DANDROID)
  set(ANDROID_PLATFORM_SRC llarp/android/ifaddrs.c)
endif()

if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
  set(ISOLATE_PROC_SRC llarp/linux/netns.cpp)
endif()


set(CXX_COMPAT_SRC
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
include_directories(vendor/cppbackport-master/lib)

set(LIB_PLATFORM_SRC
# string stuff
  llarp/str.cpp
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
# process isolation implementation
  ${ISOLATE_PROC_SRC}
# tun
  ${LIBTUNTAP_SRC}
# win32 inline code
  llarp/win32_inet.c
  llarp/win32_intrnl.c
# c++17 compat code
  ${CXX_COMPAT_SRC}
)

set(NTRU_AVX_SRC
  crypto/libntrup/src/avx/randomsmall.c
  crypto/libntrup/src/avx/weight.c
  crypto/libntrup/src/avx/swap.c
  crypto/libntrup/src/avx/rq_round3.c
  crypto/libntrup/src/avx/rq_recip3.c
  crypto/libntrup/src/avx/small.c
  crypto/libntrup/src/avx/randomweightw.c
  crypto/libntrup/src/avx/dec.c
  crypto/libntrup/src/avx/r3_recip.c
  crypto/libntrup/src/avx/keypair.c
  crypto/libntrup/src/avx/rq_rounded.c
  crypto/libntrup/src/avx/mult.c
  crypto/libntrup/src/avx/enc.c
  crypto/libntrup/src/avx/int32_sort.c
  crypto/libntrup/src/avx/rq.c
  crypto/libntrup/src/avx/rq_mod3.c
)

set(NTRU_REF_SRC
  crypto/libntrup/src/ref/randomsmall.c
  crypto/libntrup/src/ref/swap.c
  crypto/libntrup/src/ref/rq_round3.c
  crypto/libntrup/src/ref/rq_recip3.c
  crypto/libntrup/src/ref/small.c
  crypto/libntrup/src/ref/rq_mult.c
  crypto/libntrup/src/ref/randomweightw.c
  crypto/libntrup/src/ref/random32.c
  crypto/libntrup/src/ref/dec.c
  crypto/libntrup/src/ref/r3_mult.c
  crypto/libntrup/src/ref/r3_recip.c
  crypto/libntrup/src/ref/keypair.c
  crypto/libntrup/src/ref/rq_rounded.c
  crypto/libntrup/src/ref/enc.c
  crypto/libntrup/src/ref/int32_sort.c
  crypto/libntrup/src/ref/rq.c
)

include_directories(crypto/include)

set(NTRU_SRC
  ${NTRU_AVX_SRC}
  ${NTRU_REF_SRC}
  crypto/libntrup/src/ntru.cpp
  )

set(SHA512_SRC
  crypto/sha512/sha512.c)

set(CHACHA_SRC
  crypto/chacha20/ref/chacha20_ref.c
  crypto/chacha20/dolbeau/chacha20_dolbeau-ssse3.c
  crypto/chacha20/dolbeau/chacha20_dolbeau-avx2.c
  crypto/chacha20/stream_chacha20.c
  crypto/salsa20/ref/salsa20_ref.c
  crypto/salsa20/core_salsa_ref.c
  crypto/salsa20/stream_salsa20.c
  crypto/salsa20/xmm6/salsa20_xmm6-asm.S
  crypto/salsa20/xmm6/salsa20_xmm6.c
  crypto/salsa20/xmm6int/salsa20_xmm6int-avx2.c
  crypto/salsa20/xmm6int/salsa20_xmm6int-sse2.c
  crypto/xchacha20/hchacha.c
  crypto/xchacha20/stream_xchacha20.c)
 
set(CSRNG_SRC
  crypto/csrng/randombytes_salsa20_random.c
  crypto/csrng/randombytes.c)

set(CRYPTO_MEM_SRC
  crypto/secmem/secmem.c)

set(BLAKE2B_SRC
    crypto/blake2b/blake2b-compress-avx2.c
    crypto/blake2b/blake2b-compress-ref.c
    crypto/blake2b/blake2b-compress-sse41.c
    crypto/blake2b/blake2b-compress-ssse3.c
    crypto/blake2b/blake2b-ref.c
    crypto/blake2b/generichash_blake2b.c)

set(X25519_SRC    
  crypto/curve25519/crypto_scalarmult.c
  crypto/curve25519/ref10/x25519_ref10.c
  crypto/curve25519/ref10/ed25519_ref10.c
  crypto/curve25519/sandy2x/fe51_invert.c
  crypto/curve25519/sandy2x/ladder_base.S
  crypto/curve25519/sandy2x/curve25519_sandy2x.c
  crypto/curve25519/sandy2x/consts.S
  crypto/curve25519/sandy2x/fe51_nsquare.S
  crypto/curve25519/sandy2x/fe51_mul.S
  crypto/curve25519/sandy2x/fe51_pack.S
  crypto/curve25519/sandy2x/fe_frombytes_sandy2x.c
  crypto/curve25519/sandy2x/sandy2x.S
  crypto/curve25519/sandy2x/ladder.S
  crypto/curve25519/scalarmult_curve25519.c
  crypto/ed25519/crypto_box.c
  crypto/ed25519/crypto_sign.c
  crypto/ed25519/ref10/open.c
  crypto/ed25519/ref10/obsolete.c
  crypto/ed25519/ref10/keypair.c
  crypto/ed25519/ref10/sign.c
  crypto/ed25519/sign_ed25519.c)

set(CRYPTOGRAPHY_SRC
  crypto/libsodium/init.c
  crypto/libsodium/runtime.c
  crypto/verify/crypto_verify.c
  ${CRYPTO_MEM_SRC}
  ${CSRNG_SRC}
  ${BLAKE2B_SRC}
  ${CHACHA_SRC}
  ${ED25519_SRC}
  ${X25519_SRC}
  ${SHA512_SRC}
  ${NTRU_SRC})

add_library(${CRYPTOGRAPHY_LIB} ${CRYPTOGRAPHY_SRC})


set(UTP_SRC
  libutp/utp_callbacks.cpp
  libutp/utp_utils.cpp
  libutp/utp_internal.cpp
  libutp/utp_api.cpp
  libutp/utp_packedsockaddr.cpp
  libutp/utp_hash.cpp
)

if(WIN32)
  set(UTP_SRC ${UTP_SRC} libutp/libutp_inet_ntop.cpp)
endif()


set(LIB_SRC
  ${UTP_SRC}
  llarp/address_info.cpp
  llarp/bencode.cpp
  llarp/buffer.cpp
  llarp/config.cpp
  llarp/context.cpp
  llarp/crypto_libsodium.cpp
  llarp/dht.cpp
  llarp/dns.cpp
  llarp/dnsc.cpp
  llarp/dnsd.cpp
  llarp/dns_iptracker.cpp
  llarp/dns_dotlokilookup.cpp
  llarp/encode.cpp
  llarp/encrypted_frame.cpp
  llarp/exit_info.cpp
  llarp/exit_route.cpp
  llarp/ip.cpp
  llarp/link_intro.cpp
  llarp/link_message.cpp
  llarp/net.cpp
  llarp/nodedb.cpp
  llarp/path.cpp
  llarp/pathbuilder.cpp
  llarp/pathset.cpp
  llarp/profiling.cpp
  llarp/proofofwork.cpp
  llarp/relay_commit.cpp
  llarp/relay_up_down.cpp
  llarp/router_contact.cpp
  llarp/router.cpp
  llarp/rpc.cpp
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
  llarp/dht/publish_intro.cpp
  llarp/handlers/tun.cpp
  llarp/link/curvecp.cpp
  llarp/link/server.cpp
  llarp/link/utp.cpp
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

)

set(RC_SRC
  daemon/rcutil.cpp
)

set(DNS_SRC
  llarp/dns.cpp
  llarp/dnsc.cpp
  llarp/dnsd.cpp
  llarp/dns_iptracker.cpp
  llarp/dns_dotlokilookup.cpp
  llarp/net.cpp
  daemon/dns.cpp
)

set(TEST_SRC
  test/main.cpp
  test/base32_unittest.cpp
  test/dht_unittest.cpp
  test/encrypted_frame_unittest.cpp
  test/hiddenservice_unittest.cpp
  test/pq_unittest.cpp
  test/net_unittest.cpp
  test/test_dns_unit.cpp
  test/test_dnsc_unit.cpp
  test/test_dnsd_unit.cpp
)


set(TEST_EXE testAll)
set(GTEST_DIR test/gtest)

set(CLIENT_EXE llarpc)

set(CLIENT_SRC
  client/main.cpp
)

include_directories(include)
# TODO: exclude this from includes and expose stuff properly for rcutil
include_directories(llarp)


set(RC_EXE rcutil)
set(DNS_EXE dns)


set(ALL_SRC ${CLIENT_SRC} ${RC_SRC} ${EXE_SRC} ${DNS_SRC} ${LIB_PLATFORM_SRC} ${LIB_SRC} ${TEST_SRC})

if(USE_LIBABYSS)
  set(ABYSS libabyss)

  set(ABYSS_LIB abyss)

  set(ABYSS_EXE ${ABYSS_LIB}-main)

  include_directories(${ABYSS}/include)

  set(ABYSS_SRC
    ${ABYSS}/src/http.cpp
    ${ABYSS}/src/client.cpp
    ${ABYSS}/src/server.cpp
    ${ABYSS}/src/json.cpp)

  add_library(${ABYSS_LIB} ${ABYSS_SRC})
  set(ALL_SRC ${ALL_SRC} ${ABYSS_SRC} ${ABYSS}/main.cpp)
endif()

foreach(F ${ALL_SRC})
set_source_files_properties(${F} PROPERTIES COMPILE_FLAGS -DLOG_TAG=\\\"${F}\\\")
endforeach(F)

if(SHADOW)
add_shadow_plugin(shadow-plugin-${SHARED_LIB} ${EXE_SRC} ${LIB_SRC} ${LIB_PLATFORM_SRC} ${CPP_BACKPORT_SRC} ${ABYSS_SRC} ${CRYPTOGRAPHY_SRC})
target_link_libraries(shadow-plugin-${SHARED_LIB} ${LIBS})
install(TARGETS shadow-plugin-${SHARED_LIB} DESTINATION plugins)
else()
add_executable(${RC_EXE} ${RC_SRC})
add_executable(${EXE} ${EXE_SRC})
add_executable(${CLIENT_EXE} ${CLIENT_SRC})
add_executable(${DNS_EXE} ${DNS_SRC})
add_subdirectory(${GTEST_DIR})
include_directories(${GTEST_DIR}/include ${GTEST_DIR})
add_executable(${TEST_EXE} ${TEST_SRC})


if(WITH_STATIC)
    add_library(${STATIC_LIB} STATIC ${LIB_SRC})
    add_library(${PLATFORM_LIB} STATIC ${LIB_PLATFORM_SRC})
    if(USE_LIBABYSS)
      target_link_libraries(${PLATFORM_LIB} ${THREAD_LIB} ${ABYSS_LIB})
    else()
      target_link_libraries(${PLATFORM_LIB} ${THREAD_LIB})
    endif()
    if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
      target_link_libraries(${PLATFORM_LIB} -lcap)
    endif()
    target_link_libraries(${STATIC_LIB} ${CRYPTOGRAPHY_LIB} ${LIBS} ${PLATFORM_LIB})
    if(NOT WITH_SHARED)

      target_link_libraries(${EXE} ${STATIC_LINK_LIBS} ${STATIC_LIB} ${PLATFORM_LIB})
      target_link_libraries(${CLIENT_EXE} ${STATIC_LINK_LIBS} ${STATIC_LIB} ${PLATFORM_LIB})
      target_link_libraries(${RC_EXE} ${STATIC_LINK_LIBS} ${STATIC_LIB} ${PLATFORM_LIB})
      target_link_libraries(${TEST_EXE} ${STATIC_LINK_LIBS} gtest_main ${STATIC_LIB} ${PLATFORM_LIB})
      if (WIN32)
        target_link_libraries(${EXE} ${STATIC_LINK_LIBS} ${STATIC_LIB} ${PLATFORM_LIB} ws2_32 iphlpapi)
        target_link_libraries(${CLIENT_EXE} ${STATIC_LINK_LIBS} ${STATIC_LIB} ${PLATFORM_LIB} ws2_32 iphlpapi)
        target_link_libraries(${RC_EXE} ${STATIC_LINK_LIBS} ${STATIC_LIB} ${PLATFORM_LIB} ws2_32 iphlpapi)
        target_link_libraries(${TEST_EXE} ${STATIC_LINK_LIBS} gtest_main ${STATIC_LIB} ${PLATFORM_LIB} ws2_32 iphlpapi)
      endif(WIN32)
      if (WIN32)
        target_link_libraries(${DNS_EXE} ${STATIC_LIB} ${PLATFORM_LIB} ${THREAD_LIB} ws2_32 iphlpapi)
      endif(WIN32)
      target_link_libraries(${DNS_EXE} ${STATIC_LIB} ${PLATFORM_LIB} ${THREAD_LIB})
    endif(NOT WITH_SHARED)
  endif(WITH_STATIC)
  if(ANDROID)
    add_library(${ANDROID_LIB} SHARED jni/lokinet_android.cpp)
    target_link_libraries(${ANDROID_LIB} ${STATIC_LIB} ${LIBS} log)
  endif()

  if(WITH_SHARED)
    add_library(${SHARED_LIB} SHARED ${LIB_SRC} ${LIB_PLATFORM_SRC} ${CRYPTOGRAPHY_SRC})
    if (WIN32)
      set(${LIBS} ${LIBS} ws2_32 iphlpapi)
    endif(WIN32)
    target_link_libraries(${SHARED_LIB} ${LIBS} ${THREAD_LIB})
    target_link_libraries(${EXE} ${SHARED_LIB})
    target_link_libraries(${RC_EXE} ${SHARED_LIB})
    target_link_libraries(${DNS_EXE} ${SHARED_LIB} ${THREAD_LIB})
  endif(WITH_SHARED)
endif(SHADOW)
