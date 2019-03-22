# Lowest version - android ndk 3.6.0
cmake_minimum_required(VERSION 3.6.0)

set(PROJECT_NAME lokinet)
project(${PROJECT_NAME} C CXX ASM)
set(CMAKE_C_FLAGS_RELEASE "")
set(CMAKE_CXX_FLAGS_RELEASE "")
set(CMAKE_ASM_FLAGS_RELEASE "")

# Core options
option(USE_AVX2 "enable avx2 code" )
option(USE_NETNS "enable networking namespace support. Linux only" )
option(AMD_RYZEN_HACK "hack for AMD Ryzen FPU bug (support FMA3 and FMA4 in FPU, but does not show in CPUID)" )
option(STATIC_LINK_RUNTIME "link statically against compiler runtime, standard library and pthreads")
option(NON_PC_TARGET "non-pc target build: iphone, andriod, embedded non-i386 SBC, etc" )
option(SHADOW "use shadow testing framework. linux only" )
option(ASAN "use address sanitiser, if your system has it" )
option(JEMALLOC "use jemalloc. Not required on BSD" )
option(DEBIAN "build for debian" )
option(TESTNET "testnet build" )
option(WITH_SHARED "build shared library")
option(WITH_COVERAGE "generate coverage data")
option(WARNINGS_AS_ERRORS "treat all warnings as errors. turn off for development, on for release" OFF)

# Basic definitions
get_filename_component(CORE_INCLUDE include ABSOLUTE)
get_filename_component(ABYSS_INCLUDE "${CMAKE_CURRENT_LIST_DIR}/${ABYSS}/include" ABSOLUTE)

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_PROGRAM}")
endif()

if(WIN32)
  set(CMAKE_CXX_STANDARD 17)
  ENABLE_LANGUAGE(RC)
  set(CMAKE_CXX_STANDARD_LIBRARIES "${CMAKE_CXX_STANDARD_LIBRARIES} -lshlwapi")
else()
  set(CMAKE_CXX_STANDARD 14)
endif(WIN32)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (NOT ${CMAKE_SYSTEM_NAME} MATCHES "Linux" AND SHADOW)
  message( FATAL_ERROR "shadow-framework is Linux only" )
endif(NOT ${CMAKE_SYSTEM_NAME} MATCHES "Linux" AND SHADOW)

# might be redundant. -rick
if (STATIC_LINK_RUNTIME AND SHADOW)
  message(FATAL_ERROR "the shadow-framework build cannot be linked statically!")
endif(STATIC_LINK_RUNTIME AND SHADOW)

if (WIN32 AND NOT STATIC_LINK_RUNTIME)
  message("must ship compiler runtime libraries with this build: libwinpthread-1.dll, libgcc_s_dw2-1.dll, and libstdc++-6.dll")
  message("for release builds, turn on STATIC_LINK_RUNTIME in cmake options")
endif(WIN32 AND NOT STATIC_LINK_RUNTIME)

add_subdirectory(vendor/nlohmann)

# still need the headers unconditionally
set(ABSEIL_DIR vendor/abseil-cpp)
include_directories(${ABSEIL_DIR})
if (NOT WIN32)
  add_compile_options(-fPIC)
endif(NOT WIN32)
add_subdirectory(${ABSEIL_DIR})

# turns off those annoying warnings for
# target-specific crypto code paths not
# applicable to the host's FPU -rick
if (WARNINGS_AS_ERRORS)
  add_compile_options(-Werror)
endif()
add_compile_options(-Wall -Wextra -Wno-unknown-pragmas -Wno-unknown-warning-option)
# vla are evil
add_compile_options(-Wvla)
add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-fpermissive>)
add_compile_options(-Wno-unused-function -Wno-deprecated-declarations -Wno-unknown-pragmas)

if (USING_CLANG)
  add_compile_options(-Wthread-safety)
endif()

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
    option(NO_LIBGCC "use libunwind+compiler-rt instead, must already be installed in mingw-w64 sysroot" OFF)
    add_compile_options(-Wno-unused-command-line-argument -Wno-c++11-narrowing)
    add_compile_options($<$<COMPILE_LANGUAGE:C>:-Wno-bad-function-cast>)
    if (NO_LIBGCC)
      set(CMAKE_CXX_STANDARD_LIBRARIES "-lunwind -lpsapi ${CMAKE_CXX_STANDARD_LIBRARIES}")
      set(CMAKE_C_STANDARD_LIBRARIES "-lunwind -lpsapi ${CMAKE_C_STANDARD_LIBRARIES}")
    endif(NO_LIBGCC)
  else()
    # found it. this is GNU only
    add_compile_options(-Wno-cast-function-type)
  endif(USING_CLANG)
endif(WOW64_CROSS_COMPILE OR WIN64_CROSS_COMPILE)

if(WIN32)
  add_compile_options($<$<COMPILE_LANGUAGE:C>:-Wno-bad-function-cast>)
  add_compile_options($<$<COMPILE_LANGUAGE:C>:-Wno-cast-function-type>)
# unlike unix where you get a *single* compiler ID string in .comment
# GNU ld sees fit to merge *all* the .ident sections in object files
# to .r[o]data section one after the other!
  add_compile_options(-fno-ident)
  set(FS_LIB stdc++fs)
endif(WIN32)

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

set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads REQUIRED)

if(STATIC_LINK_RUNTIME)
  add_compile_options(-static)
  if(USING_CLANG)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static -static-libstdc++ -pthread" )
  else()
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libstdc++ -static-libgcc -static -Wl,--whole-archive -lpthread -Wl,--no-whole-archive" )
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")
  endif(USING_CLANG)
endif(STATIC_LINK_RUNTIME)

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
  set(WITH_SHARED ON)
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
  add_compile_options(-fno-inline -fno-strict-aliasing )
  add_definitions(-DTESTNET=1)
  add_definitions(-DSHADOW_TESTNET)
  include_directories(${SHADOW_ROOT}/include)
endif(SHADOW)

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

add_compile_options(${OPTIMIZE_FLAGS} ${CRYPTO_FLAGS})

if(NOT GIT_VERSION)
  exec_program("git" ${CMAKE_CURRENT_SOURCE_DIR} ARGS "rev-parse --short HEAD" OUTPUT_VARIABLE GIT_VERSION)
  add_definitions(-DGIT_REV="${GIT_VERSION}")
endif(NOT GIT_VERSION)

if(RELEASE_MOTTO)
  add_definitions(-DLLARP_RELEASE_MOTTO="${RELEASE_MOTTO}")
  if(WIN32)
    add_definitions(-DRELEASE_MOTTO=${RELEASE_MOTTO})
  endif(WIN32)
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
set(LIBS ${MALLOC_LIB} ${FS_LIB})

if(ANDROID)
  list(APPEND LIBS log)
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
set(ABYSS libabyss)
set(ABYSS_LIB abyss)
set(ABYSS_EXE ${ABYSS_LIB}-main)
get_filename_component(TT_ROOT "vendor/libtuntap-master" ABSOLUTE)
add_definitions(-D${CMAKE_SYSTEM_NAME})

function(add_import_library libname)
  add_library(libname SHARED IMPORTED)
  if(NOT TARGET libname)
    message(FATAL "unable to find library ${libname}")
  endif()
endfunction()

if(UNIX)
  add_definitions(-DUNIX)
  add_definitions(-DPOSIX)
  if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
      get_filename_component(LIBTUNTAP_IMPL ${TT_ROOT}/tuntap-unix-linux.c ABSOLUTE)
      get_filename_component(EV_SRC "llarp/ev/ev_epoll.cpp" ABSOLUTE)
  elseif(${CMAKE_SYSTEM_NAME} MATCHES "Android")
  get_filename_component(LIBTUNTAP_IMPL ${TT_ROOT}/tuntap-unix-linux.c ABSOLUTE)
      get_filename_component(EV_SRC "llarp/ev/ev_epoll.cpp" ABSOLUTE)
  elseif (${CMAKE_SYSTEM_NAME} MATCHES "OpenBSD")
      set(LIBTUNTAP_IMPL ${TT_ROOT}/tuntap-unix-openbsd.c ${TT_ROOT}/tuntap-unix-bsd.c)
      get_filename_component(EV_SRC "llarp/ev/ev_kqueue.cpp" ABSOLUTE)
  elseif (${CMAKE_SYSTEM_NAME} MATCHES "NetBSD")
      set(LIBTUNTAP_IMPL ${TT_ROOT}/tuntap-unix-netbsd.c ${TT_ROOT}/tuntap-unix-bsd.c)
      get_filename_component(EV_SRC "llarp/ev/ev_kqueue.cpp" ABSOLUTE)
  elseif (${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD" OR ${CMAKE_SYSTEM_NAME} MATCHES "DragonFly")
      set(LIBTUNTAP_IMPL ${TT_ROOT}/tuntap-unix-freebsd.c ${TT_ROOT}/tuntap-unix-bsd.c)
      get_filename_component(EV_SRC "llarp/ev/ev_kqueue.cpp" ABSOLUTE)
  elseif (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
      set(LIBTUNTAP_IMPL ${TT_ROOT}/tuntap-unix-darwin.c ${TT_ROOT}/tuntap-unix-bsd.c)
      get_filename_component(EV_SRC "llarp/ev/ev_kqueue.cpp" ABSOLUTE)
# TODO: _actually_ port to solaris/illumos (it's fairly complete...except for TUN) -rick
  elseif (${CMAKE_SYSTEM_NAME} MATCHES "SunOS")
      set(LIBTUNTAP_IMPL ${TT_ROOT}/tuntap-unix-sunos.c)
      get_filename_component(EV_SRC "llarp/ev/ev_epoll.cpp" ABSOLUTE)
  else()
      message(FATAL_ERROR "Your operating system is not supported yet")
  endif()
elseif(WIN32)
    get_filename_component(LIBTUNTAP_IMPL ${TT_ROOT}/tuntap-windows.c ABSOLUTE)
    get_filename_component(EV_SRC "llarp/ev/ev_win32.cpp" ABSOLUTE)
    add_definitions(-DWIN32_LEAN_AND_MEAN -DWIN32 -DWINVER=0x500 -D_WIN32_WINNT=0x500)
else()
    message(FATAL_ERROR "What operating system _are_ you building on/for?")
endif(UNIX)

set(LIBTUNTAP_SRC_BASE
  ${TT_ROOT}/tuntap.cpp
  ${TT_ROOT}/tuntap_log.cpp
  ${LIBTUNTAP_IMPL})

if(UNIX)
  set(LIBTUNTAP_SRC ${TT_ROOT}/tuntap-unix.c ${LIBTUNTAP_SRC_BASE})
else()
  set(LIBTUNTAP_SRC ${LIBTUNTAP_SRC_BASE})
endif(UNIX)

if(NOT WIN32)
  add_subdirectory(vendor)
endif(NOT WIN32)

function(add_log_tag target)
  get_target_property(TARGET_SRCS ${target} SOURCES)
  foreach(F ${TARGET_SRCS})
    set_source_files_properties(${F} PROPERTIES COMPILE_FLAGS -DLOG_TAG=\\\"${F}\\\")
  endforeach(F)
endfunction()

set(ABYSS_SRC
  ${ABYSS}/src/md5.cpp
  ${ABYSS}/src/http.cpp
  ${ABYSS}/src/client.cpp
  ${ABYSS}/src/server.cpp)
add_library(${ABYSS_LIB} STATIC ${ABYSS_SRC})

add_subdirectory(crypto)
add_subdirectory(libutp)
add_subdirectory(llarp)

target_link_libraries(${ABYSS_LIB} PUBLIC ${PLATFORM_LIB})

if (NOT WIN32)
  add_executable(${ABYSS_EXE} ${ABYSS}/main.cpp)
  target_link_libraries(${ABYSS_EXE} PUBLIC ${ABYSS_LIB} ${STATIC_LIB} Threads::Threads)
else()
  add_executable(${ABYSS_EXE} ${ABYSS}/main.cpp llarp/win32/abyss.rc)
  target_link_libraries(${ABYSS_EXE} PUBLIC ${ABYSS_LIB} ${STATIC_LIB} ws2_32)
endif(NOT WIN32)
target_include_directories(${ABYSS_LIB} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/${ABYSS}/include")
target_include_directories(${ABYSS_EXE} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/${ABYSS}/include")
# for freebsd
if(${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")
  target_include_directories(${ABYSS_LIB} /usr/local/include)
endif(${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")
add_log_tag(${ABYSS_EXE})
add_log_tag(${ABYSS_LIB})

if (WIN32)
  set(EXE_LIBS ${STATIC_LIB} ws2_32 iphlpapi)
else()
  set(EXE_LIBS ${STATIC_LIB} cppbackport libutp)
endif(WIN32)


if(SHADOW)
  set(LOKINET_SHADOW shadow-plugin-${SHARED_LIB})
  set(LOKINET_SHADOW_LIBS ${SHARED_LIB})
  add_shadow_plugin(${LOKINET_SHADOW} ${EXE_SRC})
  target_link_libraries(${LOKINET_SHADOW} ${LOKINET_SHADOW_LIBS})
  target_include_directories(${LOKINET_SHADOW} PUBLIC ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/llarp ${PROJECT_SOURCE_DIR}/crypto/include ${PROJECT_SOURCE_DIR}/vendor/cppbackport-master/lib)
else()
  if(NOT WIN32)
    add_executable(${EXE} ${EXE_SRC})
  else()
    add_executable(${EXE} ${EXE_SRC} llarp/win32/version.rc)
  endif(NOT WIN32)

  add_log_tag(${EXE})

  install(TARGETS ${EXE} RUNTIME DESTINATION bin)
  if(WIN32)
    install(PROGRAMS ${CMAKE_SOURCE_DIR}/lokinet-bootstrap.exe DESTINATION bin)
  else()
    install(PROGRAMS ${CMAKE_SOURCE_DIR}/lokinet-bootstrap DESTINATION bin)
  endif()

  if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    install(CODE "execute_process(COMMAND setcap cap_net_admin,cap_net_bind_service=+eip ${CMAKE_INSTALL_PREFIX}/bin/lokinet)")
  endif(${CMAKE_SYSTEM_NAME} MATCHES "Linux")

  target_link_libraries(${EXE} PUBLIC ${EXE_LIBS})

  if(ANDROID)
    add_library(${ANDROID_LIB} SHARED jni/lokinet_android.cpp)
    add_log_tag(${ANDROID_LIB})
    target_link_libraries(${ANDROID_LIB} ${STATIC_LIB} ${LIBS})
  endif(ANDROID)
endif(SHADOW)

enable_testing()

if (NOT SHADOW)
  add_subdirectory(test)
endif()
