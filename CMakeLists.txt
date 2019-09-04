# Lowest version - android ndk 3.6.0
cmake_minimum_required(VERSION 3.6.0)

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
  set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_PROGRAM}")
endif()

set(PROJECT_NAME lokinet)
project(${PROJECT_NAME} C CXX)

# Core options
option(USE_AVX2 "enable avx2 code" )
option(USE_NETNS "enable networking namespace support. Linux only" )
option(AMD_RYZEN_HACK "hack for AMD Ryzen FPU bug (support FMA3 and FMA4 in FPU, but does not show in CPUID)" )
option(NATIVE_BUILD "optimise for host system and FPU, may not be portable" )
option(EMBEDDED_CFG "optimise for older hardware or embedded systems")
if (NOT MSVC)
option(STATIC_LINK_RUNTIME "link statically against compiler runtime, standard library and pthreads")
endif()
option(NON_PC_TARGET "non-pc target build: iphone, andriod, embedded non-i386 SBC, etc" )
option(SHADOW "use shadow testing framework. linux only" )
option(ASAN "use address sanitiser, if your system has it" )
option(JEMALLOC "use jemalloc. Not required on BSD" )
option(DEBIAN "build for debian" )
option(TESTNET "testnet build" )
option(WITH_SHARED "build shared library")
option(WITH_COVERAGE "generate coverage data")
option(USE_SHELLHOOKS "enable shell hooks on compile time (dangerous)" OFF)
option(WARNINGS_AS_ERRORS "treat all warnings as errors. turn off for development, on for release" OFF)

include(cmake/target_link_libraries_system.cmake)
include(cmake/add_import_library.cmake)
include(cmake/add_log_tag.cmake)
include(cmake/libatomic.cmake)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

# Basic definitions
set(LIB lokinet)
set(SHARED_LIB ${LIB}-shared)
set(STATIC_LIB ${LIB}-static)
set(CRYPTOGRAPHY_LIB ${LIB}-cryptography)
set(UTIL_LIB ${LIB}-util)
set(PLATFORM_LIB ${LIB}-platform)
set(ANDROID_LIB ${LIB}android)
set(ABYSS libabyss)
set(ABYSS_LIB abyss)
set(ABYSS_EXE ${ABYSS_LIB}-main)
get_filename_component(TT_ROOT "vendor/libtuntap-master" ABSOLUTE)
add_definitions(-D${CMAKE_SYSTEM_NAME})

get_filename_component(CORE_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/include" ABSOLUTE)
get_filename_component(ABYSS_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/${ABYSS}/include" ABSOLUTE)

if(MSVC_VERSION)
  enable_language(ASM_MASM)
  list(APPEND CMAKE_ASM_MASM_SOURCE_FILE_EXTENSIONS s)
  add_definitions(-D_WIN32_WINNT=0x0600 -DNOMINMAX -DSODIUM_STATIC)
else()
  enable_language(ASM)
endif(MSVC_VERSION)

include(cmake/solaris.cmake)
include(cmake/unix.cmake)
include(cmake/win32.cmake)

if(WIN32)
  set(CMAKE_CXX_STANDARD 17)
else()
  set(CMAKE_CXX_STANDARD 14)
endif(WIN32)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if (NOT ${CMAKE_SYSTEM_NAME} MATCHES "Linux" AND SHADOW)
  message( FATAL_ERROR "shadow-framework is Linux only" )
endif(NOT ${CMAKE_SYSTEM_NAME} MATCHES "Linux" AND SHADOW)

if(NOT DEBIAN AND NOT MSVC_VERSION)
  set(OPTIMIZE_FLAGS -O3)
  set(DEBUG_FLAGS -O0 -g3)
endif()

if(ASAN)
  set(DEBUG_FLAGS ${DEBUG_FLAGS} -fsanitize=address -fno-omit-frame-pointer)
  set(OPTIMIZE_FLAGS "-O0")
endif(ASAN)

if(CMAKE_BUILD_TYPE MATCHES "[Dd][Ee][Bb][Uu][Gg]")
  set(OPTIMIZE_FLAGS "")
  add_definitions(-DLOKINET_DEBUG=1)
  set(CRYPTO_FLAGS "")
  add_compile_options( ${DEBUG_FLAGS} )
  link_libraries( ${DEBUG_FLAGS} )
endif(CMAKE_BUILD_TYPE MATCHES "[Dd][Ee][Bb][Uu][Gg]")

# Add non-386 target-specific options here
if(NON_PC_TARGET)
  add_definitions(-DRPI)
  set(WITH_STATIC ON)
endif(NON_PC_TARGET)

if(WITH_SHELLHOOKS)
  add_definitions(-DENABLE_SHELLHOOKS)
endif(WITH_SHELLHOOKS)

# Always build PIC
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(ABSEIL_DIR vendor/abseil-cpp)
include_directories(SYSTEM ${ABSEIL_DIR})
add_subdirectory(vendor/cxxopts)
add_subdirectory(vendor/nlohmann)
include_directories(SYSTEM vendor/cxxopts/include)

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wno-unknown-warning-option)
endif()

if (NOT MSVC_VERSION)
	add_compile_options(-Wall -Wextra -Wno-unknown-pragmas)
	# vla are evil
	add_compile_options(-Wvla)
	add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-fpermissive>)
	add_compile_options(-Wno-unused-function -Wno-deprecated-declarations -Wno-unknown-pragmas)
endif()

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wthread-safety)
endif()

include(cmake/coverage.cmake)

# these vars are set by the cmake toolchain spec
if (WOW64_CROSS_COMPILE OR WIN64_CROSS_COMPILE)
  include(cmake/cross_compile.cmake)
endif(WOW64_CROSS_COMPILE OR WIN64_CROSS_COMPILE)

if(DEBIAN)
  add_definitions(-DDEBIAN)
elseif(NOT ANDROID AND NOT NON_PC_TARGET)
  if (NOT USE_AVX2)
    # Public binary releases
    set(CRYPTO_FLAGS -march=nocona -mtune=core2 -mfpmath=sse)
  else()
    set(CRYPTO_FLAGS -march=haswell -mtune=native -mfpmath=sse)
  endif()
endif()

# only needed if using AVX2
if(AMD_RYZEN_HACK AND USE_AVX2)
  set(CRYPTO_FLAGS -march=native -mfpmath=sse -mavx -mavx2 -mfma)
  message(WARNING "This option may be removed in a future release. Contact your computer manufacturer for updated ROMs or microcode patches.")
endif(AMD_RYZEN_HACK AND USE_AVX2)

if(NATIVE_BUILD)
  message(WARNING "May fail at runtime if the floating-point unit on the target system does not implement required features, some platforms will check this for you")
  set(CRYPTO_FLAGS -march=native -mfpmath=sse -mtune=native)
endif()

if(EMBEDDED_CFG)
  message(WARNING "This configuration is optimised for older hardware and/or constrained node operation, may result in poor performance on desktop systems")
  message(WARNING "For deployment on such systems, all external code (currently, libuv) must also be compiled for the target!")
  set(CRYPTO_FLAGS -march=i486 -mtune=i486)
endif()

if (NOT MSVC OR NOT MSVC_VERSION)
  add_compile_options(${OPTIMIZE_FLAGS} ${CRYPTO_FLAGS})
endif()
add_subdirectory(${ABSEIL_DIR})
add_subdirectory(vendor/gtest)

if (FS_LIB STREQUAL "cppbackport")
  add_subdirectory(vendor)
endif()

set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads REQUIRED)

include(cmake/static_link.cmake)

if(USE_NETNS)
  add_definitions(-DNETNS=1)
else()
  add_definitions(-DNETNS=0)
endif(USE_NETNS)

if(TESTNET)
  add_definitions(-DTESTNET=1)
endif(TESTNET)

if(SHADOW)
  include(cmake/shadow.cmake)
endif(SHADOW)

if(NOT GIT_VERSION)
  exec_program("git" ${CMAKE_CURRENT_SOURCE_DIR} ARGS "rev-parse --short HEAD" OUTPUT_VARIABLE GIT_VERSION_UNSTRIP)
  string(STRIP "${GIT_VERSION_UNSTRIP}" GIT_VERSION)
endif(NOT GIT_VERSION)

string(REGEX REPLACE ^fatal.*$ nogit GIT_VERSION_REAL "${GIT_VERSION}")
add_definitions("-DGIT_REV=\"${GIT_VERSION_REAL}\"")

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

if(ANDROID)
  list(APPEND LIBS log)
  add_definitions(-DANDROID)
  set(ANDROID_PLATFORM_SRC android/ifaddrs.c)
endif(ANDROID)

set(LIBTUNTAP_SRC_BASE
  ${TT_ROOT}/tuntap.cpp
  ${TT_ROOT}/tuntap_log.cpp
  ${LIBTUNTAP_IMPL})

if(UNIX)
  set(LIBTUNTAP_SRC ${TT_ROOT}/tuntap-unix.c ${LIBTUNTAP_SRC_BASE})
else()
  set(LIBTUNTAP_SRC ${LIBTUNTAP_SRC_BASE})
endif()

set(LIBS ${MALLOC_LIB} ${FS_LIB} ${LIBUV_LIBRARY})

add_subdirectory(crypto)
add_subdirectory(libutp)
add_subdirectory(llarp)
add_subdirectory(libabyss)

if (NOT WIN32)
  add_executable(${ABYSS_EXE} ${ABYSS}/main.cpp)
  target_link_libraries(${ABYSS_EXE} PUBLIC ${ABYSS_LIB} Threads::Threads ${LIBS})
elseif(NOT MSVC_VERSION)
  add_executable(${ABYSS_EXE} ${ABYSS}/main.cpp llarp/win32/abyss.rc)
  target_link_libraries(${ABYSS_EXE} PUBLIC ${ABYSS_LIB} ${STATIC_LIB} ws2_32)
else()
  add_executable(${ABYSS_EXE} ${ABYSS}/main.cpp)
  target_link_libraries(${ABYSS_EXE} PUBLIC ${ABYSS_LIB} ${STATIC_LIB} ws2_32)
endif(NOT WIN32)

# Why does abyss not inherit the existing include folders?
target_include_directories(${ABYSS_LIB} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/${ABYSS}/include" llarp vendor/nlohmann/include include crypto/include)
target_include_directories(${ABYSS_EXE} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/${ABYSS}/include" llarp vendor/nlohmann/include include crypto/include)

# for freebsd
if(${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")
  target_link_directories(${ABYSS_EXE} PRIVATE /usr/local/lib)
  target_include_directories(${ABYSS_LIB} SYSTEM PUBLIC /usr/local/include)
endif(${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")
add_log_tag(${ABYSS_EXE})
add_log_tag(${ABYSS_LIB})


if(SHADOW)
  set(LOKINET_SHADOW shadow-plugin-${SHARED_LIB})
  set(LOKINET_SHADOW_LIBS ${SHARED_LIB})
  add_shadow_plugin(${LOKINET_SHADOW} ${EXE_SRC})
  target_link_libraries(${LOKINET_SHADOW} ${LOKINET_SHADOW_LIBS})
  target_include_directories(${LOKINET_SHADOW} PUBLIC ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/llarp ${PROJECT_SOURCE_DIR}/crypto/include)
else()
  if(NOT WIN32)
    add_executable(${EXE} ${EXE_SRC})
    add_executable(lokinet-rcutil daemon/rcutil.cpp)
  elseif(NOT MSVC_VERSION)
    add_executable(${EXE} ${EXE_SRC} llarp/win32/version.rc)
    add_executable(lokinet-rcutil daemon/rcutil.cpp llarp/win32/version.rc)
  else()
    add_executable(${EXE} ${EXE_SRC})
    add_executable(lokinet-rcutil daemon/rcutil.cpp)
  endif(NOT WIN32)

  add_log_tag(${EXE})
  add_log_tag(lokinet-rcutil)

  install(TARGETS ${EXE} RUNTIME DESTINATION bin)
  install(TARGETS lokinet-rcutil RUNTIME DESTINATION bin)
  if(WIN32)
    install(PROGRAMS ${CMAKE_SOURCE_DIR}/lokinet-bootstrap.exe DESTINATION bin)
  else()
    install(PROGRAMS ${CMAKE_SOURCE_DIR}/lokinet-bootstrap DESTINATION bin)
  endif()

  if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    install(CODE "execute_process(COMMAND setcap cap_net_admin,cap_net_bind_service=+eip ${CMAKE_INSTALL_PREFIX}/bin/lokinet)")
  endif(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
  if(${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")
    target_link_directories(${EXE} PRIVATE /usr/local/lib)
    target_link_directories(lokinet-rcutil PRIVATE /usr/local/lib)
  endif()
  target_link_libraries(${EXE} PUBLIC ${EXE_LIBS} ${LIBS})
  target_link_libraries(lokinet-rcutil PUBLIC ${EXE_LIBS} ${LIBS})

  if(ANDROID)
    add_library(${ANDROID_LIB} SHARED jni/lokinet_android.cpp)
    set_property(TARGET ${ANDROID_LIB} PROPERTY CXX_STANDARD 14)
    add_log_tag(${ANDROID_LIB})
    target_link_libraries(${ANDROID_LIB} ${STATIC_LIB} ${LIBS})
  endif(ANDROID)
endif(SHADOW)

enable_testing()

if (NOT SHADOW)
  add_subdirectory(test)
endif()
