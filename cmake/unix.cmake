if(NOT UNIX)
  return()
endif()

include(CheckCXXSourceCompiles)
include(CheckLibraryExists)

add_definitions(-DUNIX)
add_definitions(-DPOSIX)

if (STATIC_LINK_RUNTIME)
  set(LIBUV_USE_STATIC ON)
endif()

if(LIBUV_ROOT)
  add_subdirectory(${LIBUV_ROOT})
  set(LIBUV_INCLUDE_DIRS ${LIBUV_ROOT}/include)
  set(LIBUV_LIBRARY uv_a)
  add_definitions(-D_LARGEFILE_SOURCE)
  add_definitions(-D_FILE_OFFSET_BITS=64)
else()
  find_package(LibUV 1.28.0 REQUIRED)
endif()

include_directories(${LIBUV_INCLUDE_DIRS})

function(check_working_cxx_atomics64 varname)
  set(OLD_CMAKE_REQUIRED_FLAGS ${CMAKE_REQUIRED_FLAGS})
  set(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} -std=c++14")
  check_cxx_source_compiles("
#include <atomic>
#include <cstdint>
std::atomic<uint64_t> x (0);
int main() {
  uint64_t i = x.load(std::memory_order_relaxed);
  return 0;
}
" ${varname})
  set(CMAKE_REQUIRED_FLAGS ${OLD_CMAKE_REQUIRED_FLAGS})
endfunction()

function(link_libatomic)
  check_working_cxx_atomics64(HAVE_CXX_ATOMICS64_WITHOUT_LIB)

  if(HAVE_CXX_ATOMICS64_WITHOUT_LIB)
    message(STATUS "Have working 64bit atomics")
    return()
  endif()

  check_library_exists(atomic __atomic_load_8 "" HAVE_CXX_LIBATOMICS64)
  if (HAVE_CXX_LIBATOMICS64)
    message(STATUS "Have 64bit atomics via library")
    list(APPEND CMAKE_REQUIRED_LIBRARIES "atomic")
    check_working_cxx_atomics64(HAVE_CXX_ATOMICS64_WITH_LIB)
    if (HAVE_CXX_ATOMICS64_WITH_LIB)
      message(STATUS "Can link with libatomic")
      link_libraries(-latomic)
      return()
    endif()
  endif()

  message(FATAL_ERROR "Host compiler must support 64-bit std::atomic!")
endfunction()

if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
  set(FS_LIB stdc++fs)
  get_filename_component(LIBTUNTAP_IMPL ${TT_ROOT}/tuntap-unix-linux.c ABSOLUTE)

  link_libatomic()
elseif(${CMAKE_SYSTEM_NAME} MATCHES "Android")
  find_library(FS_LIB NAMES c++fs c++experimental stdc++fs)
  if(FS_LIB STREQUAL FS_LIB-NOTFOUND)
    add_subdirectory(vendor)
    include_directories("${CMAKE_CURRENT_LIST_DIR}/../vendor/cppbackport-master/lib")
    add_definitions(-DLOKINET_USE_CPPBACKPORT)
    set(FS_LIB cppbackport)
  endif()
  get_filename_component(LIBTUNTAP_IMPL ${TT_ROOT}/tuntap-unix-linux.c ABSOLUTE)
elseif (${CMAKE_SYSTEM_NAME} MATCHES "OpenBSD")
  set(LIBTUNTAP_IMPL ${TT_ROOT}/tuntap-unix-openbsd.c ${TT_ROOT}/tuntap-unix-bsd.c)
elseif (${CMAKE_SYSTEM_NAME} MATCHES "NetBSD")
  set(LIBTUNTAP_IMPL ${TT_ROOT}/tuntap-unix-netbsd.c ${TT_ROOT}/tuntap-unix-bsd.c)
elseif (${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD" OR ${CMAKE_SYSTEM_NAME} MATCHES "DragonFly")
  find_library(FS_LIB NAMES c++experimental)
  set(LIBTUNTAP_IMPL ${TT_ROOT}/tuntap-unix-freebsd.c ${TT_ROOT}/tuntap-unix-bsd.c)
elseif (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  find_library(FS_LIB NAMES c++fs c++experimental stdc++fs)
  if(FS_LIB STREQUAL FS_LIB-NOTFOUND)
    add_subdirectory(vendor)
    include_directories("${CMAKE_CURRENT_LIST_DIR}/../vendor/cppbackport-master/lib")
    add_definitions(-DLOKINET_USE_CPPBACKPORT)
    set(FS_LIB cppbackport)
  endif()
  set(LIBTUNTAP_IMPL ${TT_ROOT}/tuntap-unix-darwin.c ${TT_ROOT}/tuntap-unix-bsd.c)
elseif (${CMAKE_SYSTEM_NAME} MATCHES "SunOS")
  set(LIBTUNTAP_IMPL ${TT_ROOT}/tuntap-unix-sunos.c)
  # Apple C++ screws up name decorations in stdc++fs, causing link to fail
  # Samsung does not build c++experimental or c++fs in their Apple libc++ pkgsrc build
  if (NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_subdirectory(vendor)
    include_directories("${CMAKE_CURRENT_LIST_DIR}/../vendor/cppbackport-master/lib")
    add_definitions(-DLOKINET_USE_CPPBACKPORT)
    set(FS_LIB cppbackport)
  else()
    set(FS_LIB stdc++fs)
  endif()
else()
  message(FATAL_ERROR "Your operating system is not supported yet")
endif()


set(EXE_LIBS ${STATIC_LIB} libutp)

if(RELEASE_MOTTO)
  add_definitions(-DLLARP_RELEASE_MOTTO="${RELEASE_MOTTO}")
endif()
