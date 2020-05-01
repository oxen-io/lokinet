if(NOT UNIX)
  return()
endif()

include(CheckCXXSourceCompiles)
include(CheckLibraryExists)

# can't have UNIX defined
# can't have the include paths for libuv either?
if(NOT IOS AND NOT ANDROID AND NOT WIN32)
  if(LIBCURL_ROOT)
    option(HTTP_ONLY "" ON)
    option(SSL_ENABLED "" OFF)
    option(BUILD_SHARED_LIBS "" OFF)
    option(ENABLE_IPV6 "" OFF)
    option(CMAKE_USE_OPENSSL "" OFF)
    option(CURL_CA_PATH "" OFF)
    set(CURL_CA_PATH none)
    message(STATUS "using local curl: ${LIBCURL_ROOT}")
    add_subdirectory(${LIBCURL_ROOT})
    set(CURL_INCLUDE_DIRS ${LIBCURL_ROOT}/include)
    set(CURL_LIBRARIES libcurl)
    set(CURL_FOUND TRUE)
  else()
    include(FindCURL)
  endif()
endif()

add_definitions(-DUNIX)
add_definitions(-DPOSIX)
list(APPEND LIBTUNTAP_SRC ${TT_ROOT}/tuntap-unix.c)

if (STATIC_LINK_RUNTIME OR STATIC_LINK)
  set(LIBUV_USE_STATIC ON)
endif()

if(LIBUV_ROOT)
  add_subdirectory(${LIBUV_ROOT})
  set(LIBUV_INCLUDE_DIRS ${LIBUV_ROOT}/include)
  set(LIBUV_LIBRARY uv_a)
  add_definitions(-D_LARGEFILE_SOURCE)
  add_definitions(-D_FILE_OFFSET_BITS=64)
elseif(NOT LIBUV_IN_SOURCE)
  find_package(LibUV 1.28.0 REQUIRED)
endif()

include_directories(${LIBUV_INCLUDE_DIRS})

if(EMBEDDED_CFG OR ${CMAKE_SYSTEM_NAME} MATCHES "Linux")
  link_libatomic()
endif()

if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
  list(APPEND LIBTUNTAP_SRC ${TT_ROOT}/tuntap-unix-linux.c)
elseif(${CMAKE_SYSTEM_NAME} MATCHES "Android")
  list(APPEND LIBTUNTAP_SRC ${TT_ROOT}/tuntap-unix-linux.c)
elseif (${CMAKE_SYSTEM_NAME} MATCHES "OpenBSD")
  add_definitions(-D_BSD_SOURCE)
  add_definitions(-D_GNU_SOURCE)
  add_definitions(-D_XOPEN_SOURCE=700)
  list(APPEND LIBTUNTAP_SRC ${TT_ROOT}/tuntap-unix-openbsd.c ${TT_ROOT}/tuntap-unix-bsd.c)
elseif (${CMAKE_SYSTEM_NAME} MATCHES "NetBSD")
  list(APPEND LIBTUNTAP_SRC ${TT_ROOT}/tuntap-unix-netbsd.c ${TT_ROOT}/tuntap-unix-bsd.c)
elseif (${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD" OR ${CMAKE_SYSTEM_NAME} MATCHES "DragonFly")
  list(APPEND LIBTUNTAP_SRC ${TT_ROOT}/tuntap-unix-freebsd.c ${TT_ROOT}/tuntap-unix-bsd.c)
elseif (${CMAKE_SYSTEM_NAME} MATCHES "Darwin" OR ${CMAKE_SYSTEM_NAME} MATCHES "iOS")
  list(APPEND LIBTUNTAP_SRC ${TT_ROOT}/tuntap-unix-darwin.c ${TT_ROOT}/tuntap-unix-bsd.c)
elseif (${CMAKE_SYSTEM_NAME} MATCHES "SunOS")
  list(APPEND LIBTUNTAP_SRC ${TT_ROOT}/tuntap-unix-sunos.c)
  if (LIBUV_USE_STATIC)
    link_libraries(-lkstat -lsendfile)
  endif()
else()
  message(FATAL_ERROR "Your operating system - ${CMAKE_SYSTEM_NAME} is not supported yet")
endif()

set(EXE_LIBS ${STATIC_LIB})


# Figure out if we need -lstdc++fs or -lc++fs and add it to the `filesystem` interface, if needed
# (otherwise just leave it an empty interface library; linking to it will do nothing).  The former
# is needed for gcc before v9, and the latter with libc++ before llvm v9.  But this gets more
# complicated than just using the compiler, because clang on linux by default uses libstdc++, so
# we'll just give up and see what works.

add_library(filesystem INTERFACE)

set(filesystem_code [[
#include <filesystem>

int main() {
    auto cwd = std::filesystem::current_path();
    return !cwd.string().empty();
}
]])

check_cxx_source_compiles("${filesystem_code}" filesystem_compiled)
if(filesystem_compiled)
  message(STATUS "No extra link flag needed for std::filesystem")
else()
  foreach(fslib stdc++fs c++fs)
    set(CMAKE_REQUIRED_LIBRARIES -l${fslib})
    check_cxx_source_compiles("${filesystem_code}" filesystem_compiled_${fslib})
    if (filesystem_compiled_${fslib})
      message(STATUS "Using -l${fslib} for std::filesystem support")
      target_link_libraries(filesystem INTERFACE ${fslib})
      break()
    endif()
  endforeach()
endif()
unset(CMAKE_REQUIRED_LIBRARIES)

if(RELEASE_MOTTO)
  add_definitions(-DLLARP_RELEASE_MOTTO="${RELEASE_MOTTO}")
endif()
