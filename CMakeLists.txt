cmake_minimum_required(VERSION 3.5.1)  # xenial's cmake version

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
  set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_PROGRAM}")
endif()

set(PROJECT_NAME lokinet)
project(${PROJECT_NAME} C CXX)

set(LLARP_VERSION_MAJOR 0)
set(LLARP_VERSION_MINOR 7)
set(LLARP_VERSION_PATCH 0)

add_definitions(-DLLARP_VERSION_MAJOR=${LLARP_VERSION_MAJOR})
add_definitions(-DLLARP_VERSION_MINOR=${LLARP_VERSION_MINOR})
add_definitions(-DLLARP_VERSION_PATCH=${LLARP_VERSION_PATCH})


# Core options
option(USE_AVX2 "enable avx2 code" OFF)
option(USE_NETNS "enable networking namespace support. Linux only" OFF)
option(NATIVE_BUILD "optimise for host system and FPU" ON)
option(EMBEDDED_CFG "optimise for older hardware or embedded systems" OFF)
if (WIN32)
  option(STATIC_LINK_RUNTIME "link statically against compiler runtime, standard library and pthreads" OFF)
else()
  option(STATIC_LINK "link statically against dependencies" OFF)
endif()
option(SHADOW "use shadow testing framework. linux only" OFF)
option(XSAN "use sanitiser, if your system has it" OFF)
option(JEMALLOC "use jemalloc. Not required on BSD" OFF)
option(DEBIAN "build for debian" OFF)
option(TESTNET "testnet build" OFF)
option(WITH_SHARED "build shared library" OFF)
option(WITH_COVERAGE "generate coverage data" OFF)
option(USE_SHELLHOOKS "enable shell hooks on compile time (dangerous)" OFF)
option(WARNINGS_AS_ERRORS "treat all warnings as errors. turn off for development, on for release" OFF)
option(TRACY_ROOT "include tracy profiler source" OFF)
option(WITH_TESTS "build unit tests" ON)
#option(WITH_SYSTEMD ...) defined below

include(cmake/target_link_libraries_system.cmake)
include(cmake/add_import_library.cmake)
include(cmake/add_log_tag.cmake)
include(cmake/libatomic.cmake)

if (STATIC_LINK AND STATIC_LINK_RUNTIME)
  message(FATAL "Cannot set both STATIC_LINK and STATIC_LINK_RUNTIME")
endif()

if (STATIC_LINK)
  set(CMAKE_FIND_LIBRARY_SUFFIXES ${CMAKE_STATIC_LIBRARY_SUFFIX})
  message(STATUS "setting static library suffix search")
endif()

include(cmake/basic_definitions.cmake)

if(MSVC_VERSION)
  enable_language(ASM_MASM)
  list(APPEND CMAKE_ASM_MASM_SOURCE_FILE_EXTENSIONS s)
  add_definitions(-D_WIN32_WINNT=0x0600 -DNOMINMAX -DSODIUM_STATIC)
else()
  enable_language(ASM)
endif(MSVC_VERSION)

include(cmake/solaris.cmake)
include(cmake/win32.cmake)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

# No in-source building
include(MacroEnsureOutOfSourceBuild)
macro_ensure_out_of_source_build("${PROJECT_NAME} requires an out-of-source build.  Create a build directory and run 'cmake ${CMAKE_SOURCE_DIR} [options]'.")

include(cmake/unix.cmake)

if(NOT WIN32)
  if(IOS OR ANDROID)
    set(NON_PC_TARGET ON)
  else()
    include(TargetArch)
    target_architecture(COMPILE_ARCH)
    if(COMPILE_ARCH MATCHES i386 OR COMPILE_ARCH MATCHES x86_64)
      set(NON_PC_TARGET OFF)
    else()
      set(NON_PC_TARGET ON)
    endif()
  endif()
endif()

set(CMAKE_CXX_STANDARD 14)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# this is messing with release builds
add_compile_options(-U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0)

if (NOT ${CMAKE_SYSTEM_NAME} MATCHES "Linux" AND SHADOW)
  message( FATAL_ERROR "shadow-framework is Linux only" )
endif(NOT ${CMAKE_SYSTEM_NAME} MATCHES "Linux" AND SHADOW)

if(NOT DEBIAN AND NOT MSVC_VERSION)
  set(OPTIMIZE_FLAGS -O3)
  set(DEBUG_FLAGS -O0 -g3)
endif()

if(XSAN)
  set(DEBUG_FLAGS ${DEBUG_FLAGS} "-fsanitize=${XSAN}" -fno-omit-frame-pointer)
  set(OPTIMIZE_FLAGS "-O0")
  message(STATUS "Doing a ${XSAN} sanitizer build")
endif(XSAN)

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

if(TRACY_ROOT)
  include_directories(${TRACY_ROOT})
  add_definitions(-DTRACY_ENABLE)
endif()


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
elseif(NATIVE_BUILD)
  if(CMAKE_SYSTEM_PROCESSOR STREQUAL ppc64le)
    set(CRYPTO_FLAGS -mcpu=native -mtune=native)
  else()
    set(CRYPTO_FLAGS -march=native -mtune=native)
  endif()
elseif(NOT NON_PC_TARGET)
  if (USE_AVX2)
    set(CRYPTO_FLAGS -march=haswell -mtune=haswell -mfpmath=sse)
  else()
    # Public binary releases
    set(CRYPTO_FLAGS -march=nocona -mtune=haswell -mfpmath=sse)
  endif()
endif()

if(EMBEDDED_CFG)
  message(WARNING "This configuration is optimised for older hardware and/or constrained node operation, may result in poor performance on desktop systems")
  message(WARNING "For deployment on such systems, all external code (currently, libuv) must also be compiled for the target!")
  set(CRYPTO_FLAGS -march=i486 -mtune=i486)
endif()

add_compile_options(${OPTIMIZE_FLAGS} ${CRYPTO_FLAGS})

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
  # 5 times slower than realtime
  add_definitions(-DTESTNET_SPEED=5)
endif(TESTNET)

if(SHADOW)
  include(cmake/shadow.cmake)
endif(SHADOW)

unset(GIT_VERSION)
unset(GIT_VERSION_REAL)

if(NOT GIT_VERSION)
  exec_program("git" ${CMAKE_CURRENT_SOURCE_DIR} ARGS "rev-parse --short HEAD" OUTPUT_VARIABLE GIT_VERSION_UNSTRIP)
  string(STRIP "${GIT_VERSION_UNSTRIP}" GIT_VERSION)
endif(NOT GIT_VERSION)

string(REGEX REPLACE "^fatal.*$" nogit GIT_VERSION_REAL "${GIT_VERSION}")

# HeapAlloc(2) on Windows was significantly revamped in 2009
# but the old algorithm isn't too bad either
# this is _the_ system allocator on BSD UNIX
# openbsd replaced it with a secure/randomised malloc not too
# long ago
if(JEMALLOC)
  set(MALLOC_LIB jemalloc)
endif(JEMALLOC)


find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND AND LINUX)
    pkg_check_modules(SD libsystemd)
    # Default WITH_SYSTEMD to true if we found it
    option(WITH_SYSTEMD "enable systemd integration for sd_notify" ${SD_FOUND})

    if(WITH_SYSTEMD)
      if(NOT SD_FOUND)
        message(FATAL_ERROR "libsystemd not found")
      endif()
      add_definitions(-DWITH_SYSTEMD)
      include_directories(${SD_INCLUDE_DIRS})
      set(SD_LIBS ${SD_LDFLAGS})
    endif()
endif()

option(SUBMODULE_CHECK "Enables checking that vendored library submodules are up to date" ON)
if(SUBMODULE_CHECK)
  find_package(Git)
  if(GIT_FOUND)
    function(check_submodule relative_path)
      execute_process(COMMAND git rev-parse "HEAD" WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${relative_path} OUTPUT_VARIABLE localHead)
      execute_process(COMMAND git rev-parse "HEAD:${relative_path}" WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} OUTPUT_VARIABLE checkedHead)
      string(COMPARE EQUAL "${localHead}" "${checkedHead}" upToDate)
      if (upToDate)
        message(STATUS "Submodule '${relative_path}' is up-to-date")
      else()
        message(FATAL_ERROR "Submodule '${relative_path}' is not up-to-date. Please update with\ngit submodule update --init --recursive\nor run cmake with -DSUBMODULE_CHECK=OFF")
      endif()
    endfunction ()

    message(STATUS "Checking submodules")
    check_submodule(external/nlohmann)
    check_submodule(external/googletest)
    check_submodule(external/cxxopts)
    check_submodule(external/ghc-filesystem)
    check_submodule(external/optional-lite)
    check_submodule(external/date)
  endif()
endif()

if(WITH_TESTS)
  add_subdirectory(external/googletest EXCLUDE_FROM_ALL)
endif()

set(JSON_BuildTests OFF CACHE INTERNAL "")
add_subdirectory(external/nlohmann EXCLUDE_FROM_ALL)
add_subdirectory(external/cxxopts EXCLUDE_FROM_ALL)
add_subdirectory(external/ghc-filesystem)
add_subdirectory(external/optional-lite EXCLUDE_FROM_ALL)
add_subdirectory(external/date EXCLUDE_FROM_ALL)

if(ANDROID)
  list(APPEND LIBS log)
  add_definitions(-DANDROID)
  set(ANDROID_PLATFORM_SRC android/ifaddrs.c)
endif(ANDROID)

set(LIBS ${MALLOC_LIB} ${LIBUV_LIBRARY} ${SD_LIBS})
if(TRACY_ROOT)
  list(APPEND LIBS -ldl)
endif()

add_subdirectory(crypto)
add_subdirectory(llarp)
add_subdirectory(libabyss)
add_subdirectory(daemon)

if (NOT SHADOW)
  if(WITH_TESTS)
    add_subdirectory(test)
  endif()
  if(ANDROID)
    add_subdirectory(jni)
  endif(ANDROID)
endif()

include(cmake/installer.cmake)
