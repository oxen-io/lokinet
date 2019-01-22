# Lowest version - android ndk 3.6.0
cmake_minimum_required(VERSION 3.6.0)

set(PROJECT_NAME lokinet)
project(${PROJECT_NAME} C CXX ASM)

option(USE_LIBABYSS "enable libabyss" )
option(USE_AVX2 "enable avx2 code" )
option(USE_NETNS "enable networking namespace support" )
option(AMD_RYZEN_HACK "hack for AMD Ryzen FPU bug (support FMA3 and FMA4 in FPU, but does not show in CPUID)" )
option(STATIC_LINK "emit fully linked binaries" )
option(NON_PC_TARGET "non-pc target build: iphone, andriod, embedded non-i386 SBC, etc" )
option(SHADOW "use shadow testing framework. linux only" )
option(ASAN "use address sanitiser, if your system has it" )
option(JEMALLOC "use jemalloc. Not required on BSD" )
option(DEBIAN "build for debian" )
option(TESTNET "testnet build" )
option(WITH_SHARED "build shared library")
option(WITH_COVERAGE "generate coverage data")

if(WIN32)
  set(CMAKE_CXX_STANDARD 17)
  ENABLE_LANGUAGE(RC)
else()
  set(CMAKE_CXX_STANDARD 14)
endif(WIN32)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (NOT ${CMAKE_SYSTEM_NAME} MATCHES "Linux" AND SHADOW)
  message( FATAL_ERROR "Shared library target is Linux only" )
endif(NOT ${CMAKE_SYSTEM_NAME} MATCHES "Linux" AND SHADOW)

if (STATIC_LINK AND SHADOW)
  message(FATAL_ERROR "the shadow-framework build cannot be linked statically!")
endif(STATIC_LINK AND SHADOW)

# turns off those annoying warnings for
# target-specific crypto code paths not
# applicable to the host's FPU -rick
add_compile_options(-Wall -Wextra -Werror -Wno-unknown-pragmas -Wno-unknown-warning-option)
# vla are evil
add_compile_options(-Wvla)
add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-fpermissive>)
add_compile_options(-Wno-unused-function -Wno-deprecated-declarations -Wno-unknown-pragmas)

if (WITH_COVERAGE)
  if (USING_CLANG)
    add_compile_options( -fprofile-instr-generate -fcoverage-mapping )
    link_libraries( -fprofile-instr-generate )
  else()
    add_compile_options( --coverage -g0 )
    link_libraries( --coverage )
  endif()
endif()

# these vars are set by the cmake toolchain spec
if (WOW64_CROSS_COMPILE OR WIN64_CROSS_COMPILE)
# dynamic linking does this all the time
  if (USING_CLANG) # windows only, also set by toolchain file
    option(NO_LIBGCC "use compiler-rt instead" OFF)
    add_compile_options(-Wno-unused-command-line-argument -Wno-c++11-narrowing)
    add_compile_options($<$<COMPILE_LANGUAGE:C>:-Wno-bad-function-cast>)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--allow-multiple-definition")
    if (NO_LIBGCC)
      set(CMAKE_CXX_STANDARD_LIBRARIES "-lgcc_eh ${CMAKE_CXX_STANDARD_LIBRARIES}")
      set(CMAKE_C_STANDARD_LIBRARIES "-lgcc_eh ${CMAKE_C_STANDARD_LIBRARIES}")
    endif(NO_LIBGCC)
  else()
    # found it. this is GNU only
    add_compile_options(-Wno-cast-function-type)
  endif(USING_CLANG)
endif(WOW64_CROSS_COMPILE OR WIN64_CROSS_COMPILE)

if(WIN32)
  add_compile_options($<$<COMPILE_LANGUAGE:C>:-Wno-bad-function-cast>)
  add_compile_options($<$<COMPILE_LANGUAGE:C>:-Wno-cast-function-type>)
  set(FS_LIB stdc++fs)
endif(WIN32)

if(DEBIAN)
  add_definitions(-DDEBIAN)
else()
  if(NOT ANDROID)
    if(NOT NON_PC_TARGET)
      if (NOT USE_AVX2)
        set(CRYPTO_FLAGS -march=core2 -msse4.1 -mtune=native -mfpmath=sse)
      else()
        set(CRYPTO_FLAGS -march=haswell -mtune=native -mfpmath=sse)
      endif(NOT USE_AVX2)
    endif(NOT NON_PC_TARGET)
  endif(NOT ANDROID)
endif(DEBIAN)

# only needed if using AVX2
if(AMD_RYZEN_HACK AND USE_AVX2)
  set(CRYPTO_FLAGS -march=native -mfpmath=sse -mavx -mavx2 -mfma)
endif(AMD_RYZEN_HACK AND USE_AVX2)

if(WITH_SHARED)
  set(CRYPTO_FLAGS ${CRYPTO_FLAGS} -fPIC)
endif()

set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads REQUIRED)

if(STATIC_LINK)
  add_compile_options(-static)
  if(USING_CLANG)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static")
  else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}  -static-libgcc -static -Wl,--whole-archive -Wl,--no-whole-archive")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libstdc++ -static-libgcc -static -Wl,--whole-archive -lpthread -Wl,--no-whole-archive" )
  endif(USING_CLANG)
endif(STATIC_LINK)

if(USE_NETNS)
  add_definitions(-DNETNS=1)
else()
  add_definitions(-DNETNS=0)
endif(USE_NETNS)

if(TESTNET)
  add_definitions(-DTESTNET=1)
endif(TESTNET)

if(NOT DEBIAN)
  set(OPTIMIZE_FLAGS -O3)
  set(DEBUG_FLAGS -O0 -g3)
endif(NOT DEBIAN)

if(ASAN)
  set(DEBUG_FLAGS ${DEBUG_FLAGS} -fsanitize=thread -fno-omit-frame-pointer)
  set(OPTIMIZE_FLAGS "-O0")
endif(ASAN)

if(SHADOW)
  set(WITH_STATIC OFF)
  add_compile_options(-fPIC)
  if("${SHADOW_ROOT}" STREQUAL "")
    set(SHADOW_ROOT "$ENV{HOME}/.shadow")
  endif("${SHADOW_ROOT}" STREQUAL "")
  if(EXISTS "${SHADOW_ROOT}")
    message(STATUS "SHADOW_ROOT = ${SHADOW_ROOT}")
  else()
    message(FATAL_ERROR "SHADOW_ROOT path does not exist: '${SHADOW_ROOT}'")
  endif(EXISTS "${SHADOW_ROOT}")

  set(CMAKE_MODULE_PATH "${SHADOW_ROOT}/share/cmake/Modules")
  include_directories(${CMAKE_MODULE_PATH})
  include(ShadowTools)
  add_compile_options( -fno-inline -fno-strict-aliasing )
  add_definitions(-DTESTNET=true)
  add_definitions(-DSHADOW_TESTNET)
  include_directories(${SHADOW_ROOT}/include)
else()
  set(WITH_STATIC ON)
endif(SHADOW)

if(CMAKE_BUILD_TYPE MATCHES "[Dd][Ee][Bb][Uu][Gg]")
  set(OPTIMIZE_FLAGS "")
  add_compile_options( ${DEBUG_FLAGS} )
  link_libraries( ${DEBUG_FLAGS} )
endif(CMAKE_BUILD_TYPE MATCHES "[Dd][Ee][Bb][Uu][Gg]")

# Add non-386 target-specific options here
if(NON_PC_TARGET)
  add_definitions(-DRPI)
  set(WITH_STATIC ON)
endif(NON_PC_TARGET)

add_compile_options(${OPTIMIZE_FLAGS} ${CRYPTO_FLAGS})

if(NOT GIT_VERSION)
  exec_program("git" ${CMAKE_CURRENT_SOURCE_DIR} ARGS "rev-parse --short HEAD" OUTPUT_VARIABLE GIT_VERSION)
  add_definitions(-DGIT_REV="${GIT_VERSION}")
endif(NOT GIT_VERSION)

if(RELEASE_MOTTO)
  add_definitions(-DLLARP_RELEASE_MOTTO="${RELEASE_MOTTO}")
endif(RELEASE_MOTTO)

set(EXE lokinet)
set(EXE_SRC daemon/main.cpp)

# HeapAlloc(2) on Windows was significantly revamped in 2009
# but the old algorithm isn't too bad either
# this is _the_ system allocator on BSD UNIX
# openbsd replaced it with a secure/randomised malloc not too
# long ago
if(JEMALLOC)
  set(MALLOC_LIB jemalloc)
endif(JEMALLOC)


# FS_LIB should resolve to nothing on all other platforms
# it is only required on win32 -rick
set(LIBS ${LIBS} ${MALLOC_LIB} ${FS_LIB})

if(ANDROID)
  set(LIBS ${LIBS} log)
  add_definitions(-DANDROID)
  set(ANDROID_PLATFORM_SRC llarp/android/ifaddrs.c)
endif(ANDROID)

set(LIB lokinet)
set(SHARED_LIB ${LIB}-shared)
set(STATIC_LIB ${LIB}-static)
set(CRYPTOGRAPHY_LIB ${LIB}-cryptography)
set(UTIL_LIB ${LIB}-util)
set(PLATFORM_LIB ${LIB}-platform)
set(ANDROID_LIB ${LIB}android)
set(TT_ROOT vendor/libtuntap-master)
add_definitions(-D${CMAKE_SYSTEM_NAME})

if(UNIX)
  add_definitions(-DUNIX)
  add_definitions(-DPOSIX)
  if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
        set(LIBTUNTAP_IMPL ${TT_ROOT}/tuntap-unix-linux.c)
        set(EV_SRC llarp/ev/ev_epoll.cpp)
    elseif(${CMAKE_SYSTEM_NAME} MATCHES "Android")
        set(LIBTUNTAP_IMPL ${TT_ROOT}/tuntap-unix-linux.c)
        set(EV_SRC llarp/ev/ev_epoll.cpp)
    elseif (${CMAKE_SYSTEM_NAME} MATCHES "OpenBSD")
        set(LIBTUNTAP_IMPL ${TT_ROOT}/tuntap-unix-openbsd.c ${TT_ROOT}/tuntap-unix-bsd.c)
        set(EV_SRC llarp/ev/ev_kqueue.cpp)
    elseif (${CMAKE_SYSTEM_NAME} MATCHES "NetBSD")
        set(LIBTUNTAP_IMPL ${TT_ROOT}/tuntap-unix-netbsd.c ${TT_ROOT}/tuntap-unix-bsd.c)
        set(EV_SRC llarp/ev/ev_kqueue.cpp)
    elseif (${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD" OR ${CMAKE_SYSTEM_NAME} MATCHES "DragonFly")
        set(LIBTUNTAP_IMPL ${TT_ROOT}/tuntap-unix-freebsd.c ${TT_ROOT}/tuntap-unix-bsd.c)
        set(EV_SRC llarp/ev/ev_kqueue.cpp)
    elseif (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
        set(LIBTUNTAP_IMPL ${TT_ROOT}/tuntap-unix-darwin.c ${TT_ROOT}/tuntap-unix-bsd.c)
        set(EV_SRC llarp/ev/ev_kqueue.cpp)
# TODO: _actually_ port to solaris/illumos (it's fairly complete...except for TUN) -rick
    elseif (${CMAKE_SYSTEM_NAME} MATCHES "SunOS")
        set(LIBTUNTAP_IMPL ${TT_ROOT}/tuntap-unix-sunos.c)
        set(EV_SRC llarp/ev/ev_epoll.cpp)
    else()
        message(FATAL_ERROR "Your operating system is not supported yet")
    endif(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
elseif(WIN32)
    set(LIBTUNTAP_IMPL ${TT_ROOT}/tuntap-windows.c)
    set(EV_SRC llarp/ev/ev_win32.cpp)
    add_definitions(-DWIN32_LEAN_AND_MEAN -DWIN32 -DWINVER=0x500 -D_WIN32_WINNT=0x500)
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

if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
  set(ISOLATE_PROC_SRC llarp/linux/netns.cpp)
endif(${CMAKE_SYSTEM_NAME} MATCHES "Linux")

if(NOT WIN32)
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
endif(NOT WIN32)

set(LIB_UTIL_SRC
  llarp/constants/defaults.cpp
  llarp/constants/link_layer.cpp
  llarp/constants/proto.cpp
  llarp/constants/version.cpp
  llarp/util/aligned.cpp
  llarp/util/bencode.cpp
  llarp/util/bits.cpp
  llarp/util/buffer.cpp
  llarp/util/codel.cpp
  llarp/util/common.cpp
  llarp/util/encode.cpp
  llarp/util/endian.cpp
  llarp/util/fs.cpp
  llarp/util/ini.cpp
  llarp/util/logger.c
  llarp/util/logger.cpp
  llarp/util/logic.cpp
  llarp/util/mem.cpp
  llarp/util/queue_manager.cpp
  llarp/util/queue.cpp
  llarp/util/str.cpp
  llarp/util/string_view.cpp
  llarp/util/thread_pool.cpp
  llarp/util/threading.cpp
  llarp/util/threadpool.cpp
  llarp/util/time.cpp
  llarp/util/timer.cpp
  llarp/util/types.cpp
)

set(LIB_PLATFORM_SRC
# for networking
  llarp/ev/ev.cpp
  llarp/net/net.cpp
  llarp/net/net_addr.cpp
  llarp/net/net_inaddr.cpp
# for android shim
  ${ANDROID_PLATFORM_SRC}
# process isolation implementation
  ${ISOLATE_PROC_SRC}
# tun
  ${LIBTUNTAP_SRC}
# c++17 compat code
  ${CXX_COMPAT_SRC}
# win32 inline code
  llarp/win32/win32_inet.c
  llarp/win32/win32_intrnl.c
  llarp/win32/win32_upoll.c
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

add_library(${CRYPTOGRAPHY_LIB} STATIC ${CRYPTOGRAPHY_SRC})

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
endif(WIN32)

set(DNSLIB_SRC
  llarp/dns/dotlokilookup.cpp
  llarp/dns/dns.cpp
  llarp/dns/iptracker.cpp
  llarp/dns/message.cpp
  llarp/dns/name.cpp
  llarp/dns/query.cpp
  llarp/dns/question.cpp
  llarp/dns/rectypes.cpp
  llarp/dns/rr.cpp
  llarp/dns/serialize.cpp
  llarp/dns/server.cpp
  llarp/dns/string.cpp
)


set(LIB_SRC
  ${DNSLIB_SRC}
  ${EV_SRC}
  ${UTP_SRC}
  llarp/config.cpp
  llarp/context.cpp
  llarp/crypto/constants.cpp
  llarp/crypto/crypto_libsodium.cpp
  llarp/crypto/crypto.cpp
  llarp/crypto/encrypted_frame.cpp
  llarp/crypto/encrypted.cpp
  llarp/crypto/types.cpp
  llarp/dht/bucket.cpp
  llarp/dht/context.cpp
  llarp/dht/decode.cpp
  llarp/dht/dht_immediate.cpp
  llarp/dht/dht.cpp
  llarp/dht/find_intro.cpp
  llarp/dht/find_router.cpp
  llarp/dht/got_intro.cpp
  llarp/dht/got_router.cpp
  llarp/dht/kademlia.cpp
  llarp/dht/key.cpp
  llarp/dht/message.cpp
  llarp/dht/messages/all.cpp
  llarp/dht/messages/findintro.cpp
  llarp/dht/messages/findrouter.cpp
  llarp/dht/messages/gotintro.cpp
  llarp/dht/messages/gotrouter.cpp
  llarp/dht/messages/pubintro.cpp
  llarp/dht/node.cpp
  llarp/dht/publish_intro.cpp
  llarp/dns.cpp
  llarp/dnsc.cpp
  llarp/dnsd.cpp
  llarp/exit/close_exit.cpp
  llarp/exit/context.cpp
  llarp/exit/endpoint.cpp
  llarp/exit/grant_exit.cpp
  llarp/exit/obtain_exit.cpp
  llarp/exit/policy.cpp
  llarp/exit/reject_exit.cpp
  llarp/exit/session.cpp
  llarp/exit/transfer_traffic.cpp
  llarp/exit/update_exit.cpp
  llarp/handlers/exit.cpp
  llarp/handlers/null.cpp
  llarp/handlers/tun.cpp
  llarp/link/curvecp.cpp
  llarp/link/encoder.cpp
  llarp/link/iwp.cpp
  llarp/link/server.cpp
  llarp/link/session.cpp
  llarp/link/utp.cpp
  llarp/messages/dht.cpp
  llarp/messages/dht_immediate.cpp
  llarp/messages/discard.cpp
  llarp/messages/exit.cpp
  llarp/messages/link_intro.cpp
  llarp/messages/link_message.cpp
  llarp/messages/path_confirm.cpp
  llarp/messages/path_latency.cpp
  llarp/messages/path_transfer.cpp
  llarp/messages/relay.cpp
  llarp/messages/relay_commit.cpp
  llarp/messages/transfer_traffic.cpp
  llarp/net/address_info.cpp
  llarp/net/exit_info.cpp
  llarp/net/ip.cpp
  llarp/net/net_int.cpp
  llarp/nodedb.cpp
  llarp/path/path.cpp
  llarp/path/path_types.cpp
  llarp/path/pathbuilder.cpp
  llarp/path/pathset.cpp
  llarp/path/transit_hop.cpp
  llarp/pow.cpp
  llarp/profiling.cpp
  llarp/router/router.cpp
  llarp/router_contact.cpp
  llarp/router_id.cpp
  llarp/routing/dht_message.cpp
  llarp/routing/handler.cpp
  llarp/routing/message_parser.cpp
  llarp/routing/message.cpp
  llarp/routing/path_confirm.cpp
  llarp/routing/path_latency.cpp
  llarp/routing/path_transfer.cpp
  llarp/rpc/rpc.cpp
  llarp/service/Identity.cpp
  llarp/service/Intro.cpp
  llarp/service/IntroSet.cpp
  llarp/service/address.cpp
  llarp/service/config.cpp
  llarp/service/context.cpp
  llarp/service/endpoint.cpp
  llarp/service/handler.cpp
  llarp/service/info.cpp
  llarp/service/lookup.cpp
  llarp/service/protocol.cpp
  llarp/service/tag.cpp
  llarp/service/types.cpp
  llarp/service/vanity.cpp
  llarp/testnet.c
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
  llarp/dns_rectypes.cpp
  llarp/net/net.cpp
  daemon/dns.cpp
)

set(TEST_SRC
  # helpers
  test/main.cpp
  test/test_util.cpp
  # actual test cases
  test/crypto/test_llarp_crypto_types.cpp
  test/crypto/test_llarp_crypto.cpp
  test/dht/test_llarp_dht_key.cpp
  test/dht/test_llarp_dht.cpp
  test/dns/test_llarp_dns_dns.cpp
  test/exit/test_llarp_exit_context.cpp
  test/link/test_llarp_link.cpp
  test/net/test_llarp_net_inaddr.cpp
  test/net/test_llarp_net.cpp
  test/routing/llarp_routing_transfer_traffic.cpp
  test/routing/test_llarp_routing_obtainexitmessage.cpp
  test/service/test_llarp_service_address.cpp
  test/service/test_llarp_service_identity.cpp
  test/test_llarp_dns.cpp
  test/test_llarp_dnsd.cpp
  test/test_llarp_encrypted_frame.cpp
  test/test_llarp_router_contact.cpp
  test/test_llarp_router.cpp
  test/util/test_llarp_util_aligned.cpp
  test/util/test_llarp_util_bencode.cpp
  test/util/test_llarp_util_encode.cpp
  test/util/test_llarp_util_ini.cpp
  test/util/test_llarp_util_queue_manager.cpp
  test/util/test_llarp_util_queue.cpp
  test/util/test_llarp_util_thread_pool.cpp
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
#include_directories(include)
#include_directories(vendor/cppbackport-master/lib)
#include_directories(${sodium_INCLUDE_DIR})

set(RC_EXE rcutil)
set(DNS_EXE dns)
set(ALL_SRC ${CLIENT_SRC} ${RC_SRC} ${EXE_SRC} ${DNS_SRC} ${LIB_PLATFORM_SRC} ${LIB_SRC} ${TEST_SRC})

if(USE_LIBABYSS)
  add_definitions(-DUSE_ABYSS=1)
  set(ABYSS libabyss)
  set(ABYSS_LIB abyss)
  set(ABYSS_EXE ${ABYSS_LIB}-main)
  include_directories(${ABYSS}/include)
  set(ABYSS_SRC
    ${ABYSS}/src/http.cpp
    ${ABYSS}/src/client.cpp
    ${ABYSS}/src/server.cpp
    ${ABYSS}/src/json.cpp)
  add_library(${ABYSS_LIB} STATIC ${ABYSS_SRC})
  set(ALL_SRC ${ALL_SRC} ${ABYSS_SRC} ${ABYSS}/main.cpp)
  add_executable(${ABYSS_EXE} ${ABYSS}/main.cpp)

if (NOT WIN32)
  target_link_libraries(${ABYSS_EXE} ${UTIL_LIB} ${PLATFORM_LIB})
else()
  target_link_libraries(${ABYSS_EXE} ${UTIL_LIB} ${PLATFORM_LIB} ws2_32 iphlpapi)
endif(NOT WIN32)

  set(TEST_SRC ${TEST_SRC} test/test_libabyss.cpp)
  # for freebsd
  if(${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")
    include_directories(/usr/local/include)
  endif(${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")
endif(USE_LIBABYSS)

foreach(F ${ALL_SRC})
set_source_files_properties(${F} PROPERTIES COMPILE_FLAGS -DLOG_TAG=\\\"${F}\\\")
endforeach(F)

if(SHADOW)
  add_shadow_plugin(shadow-plugin-${SHARED_LIB} ${EXE_SRC} ${LIB_SRC} ${LIB_PLATFORM_SRC} ${CPP_BACKPORT_SRC} ${ABYSS_SRC} ${CRYPTOGRAPHY_SRC})
  target_link_libraries(shadow-plugin-${SHARED_LIB} ${LIBS})
  install(TARGETS shadow-plugin-${SHARED_LIB} DESTINATION plugins)
else()
  #add_executable(${RC_EXE} ${RC_SRC})
  #add_executable(${CLIENT_EXE} ${CLIENT_SRC})
  #add_executable(${DNS_EXE} ${DNS_SRC})
  add_subdirectory(${GTEST_DIR})
if(NOT WIN32)
  add_executable(${TEST_EXE} ${TEST_SRC})
  add_executable(${EXE} ${EXE_SRC})
else()
  add_executable(${TEST_EXE} ${TEST_SRC} llarp/version.rc)
  add_executable(${EXE} ${EXE_SRC} llarp/version.rc)
endif(NOT WIN32)

  target_include_directories(${TEST_EXE} PRIVATE test)
  target_include_directories(${TEST_EXE} PRIVATE ${GTEST_DIR}/include ${GTEST_DIR})

  install(TARGETS ${EXE} RUNTIME DESTINATION bin)
  if(WIN32)
    install(PROGRAMS ${CMAKE_SOURCE_DIR}/lokinet-bootstrap.exe DESTINATION bin)
  else()
    install(PROGRAMS ${CMAKE_SOURCE_DIR}/lokinet-bootstrap DESTINATION bin)
  endif()
  if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    install(CODE "execute_process(COMMAND setcap cap_net_admin,cap_net_bind_service=+eip ${CMAKE_INSTALL_PREFIX}/bin/lokinet)")
  endif(${CMAKE_SYSTEM_NAME} MATCHES "Linux")

  add_library(${STATIC_LIB} STATIC ${LIB_SRC})
  add_library(${UTIL_LIB} STATIC ${LIB_UTIL_SRC})
  add_library(${PLATFORM_LIB} STATIC ${LIB_PLATFORM_SRC})
  if(USE_LIBABYSS)
    target_link_libraries(${PLATFORM_LIB} ${UTIL_LIB} Threads::Threads ${ABYSS_LIB})
  else()
    target_link_libraries(${PLATFORM_LIB} ${UTIL_LIB} Threads::Threads)
  endif(USE_LIBABYSS)
  if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    if(NON_PC_TARGET)
      target_link_libraries(${PLATFORM_LIB} -lrt)
    else()
      target_link_libraries(${PLATFORM_LIB} -lcap)
    endif(NON_PC_TARGET)
  endif(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
  target_link_libraries(${STATIC_LIB} ${CRYPTOGRAPHY_LIB} ${LIBS} ${UTIL_LIB} ${PLATFORM_LIB})
  target_link_libraries(${EXE} ${STATIC_LINK_LIBS} ${STATIC_LIB} ${UTIL_LIB} ${PLATFORM_LIB})
  target_link_libraries(${TEST_EXE} ${STATIC_LINK_LIBS} gtest_main ${STATIC_LIB} ${UTIL_LIB} ${PLATFORM_LIB})
  if (WIN32)
    target_link_libraries(${EXE} ${STATIC_LINK_LIBS} ${STATIC_LIB} ${UTIL_LIB} ${PLATFORM_LIB} ws2_32 iphlpapi)
    target_link_libraries(${TEST_EXE} ${STATIC_LINK_LIBS} gtest_main ${STATIC_LIB} ${UTIL_LIB} ${PLATFORM_LIB} ws2_32 iphlpapi)
  endif(WIN32)


  if(ANDROID)
    add_library(${ANDROID_LIB} SHARED jni/lokinet_android.cpp)
    target_link_libraries(${ANDROID_LIB} ${STATIC_LIB} ${LIBS})
  endif(ANDROID)

  if(WITH_SHARED)
    add_library(${SHARED_LIB} SHARED ${LIB_SRC})
    target_link_libraries(${SHARED_LIB} ${CRYPTOGRAPHY_LIB} ${LIBS} ${UTIL_LIB} ${PLATFORM_LIB} Threads::Threads)
  endif(WITH_SHARED)
endif(SHADOW)
