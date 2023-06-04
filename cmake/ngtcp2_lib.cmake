# ngtcp2's top-level CMakeLists.txt loads a bunch of crap we don't want (examples, a conflicting
# 'check' target, etc.); instead we directly include it's lib subdirectory to build just the
# library, but we have to set up a couple things to make that work:
function(add_ngtcp2_lib)
  file(STRINGS ngtcp2/CMakeLists.txt ngtcp2_project_line REGEX "^project\\(ngtcp2 ")
  if(NOT ngtcp2_project_line MATCHES "^project\\(ngtcp2 VERSION ([0-9]+)\\.([0-9]+)\\.([0-9]+)\\)$")
    message(FATAL_ERROR "Unable to extract ngtcp2 version from ngtcp2/CMakeLists.txt (found '${ngtcp2_project_line}')")
  endif()

  set(PACKAGE_VERSION "${CMAKE_MATCH_1}.${CMAKE_MATCH_2}.${CMAKE_MATCH_3}")
  include(ngtcp2/cmake/Version.cmake)
  HexVersion(PACKAGE_VERSION_NUM ${CMAKE_MATCH_1} ${CMAKE_MATCH_2} ${CMAKE_MATCH_3})
  configure_file("ngtcp2/lib/includes/ngtcp2/version.h.in" "ngtcp2/lib/includes/ngtcp2/version.h" @ONLY)

  set(BUILD_SHARED_LIBS OFF)

  # Checks for header files.
  include(CheckIncludeFile)
  check_include_file("arpa/inet.h"   HAVE_ARPA_INET_H)
  check_include_file("netinet/in.h"  HAVE_NETINET_IN_H)
  check_include_file("stddef.h"      HAVE_STDDEF_H)
  check_include_file("stdint.h"      HAVE_STDINT_H)
  check_include_file("stdlib.h"      HAVE_STDLIB_H)
  check_include_file("string.h"      HAVE_STRING_H)
  check_include_file("unistd.h"      HAVE_UNISTD_H)
  check_include_file("sys/endian.h"  HAVE_SYS_ENDIAN_H)
  check_include_file("endian.h"      HAVE_ENDIAN_H)
  check_include_file("byteswap.h"    HAVE_BYTESWAP_H)

  include(CheckTypeSize)
  check_type_size("ssize_t" SIZEOF_SSIZE_T)
  if(SIZEOF_SSIZE_T STREQUAL "")
    set(ssize_t ptrdiff_t)
  endif()

  include(CheckSymbolExists)
  if(HAVE_ENDIAN_H)
    check_symbol_exists(be64toh "endian.h" HAVE_BE64TOH)
  endif()
  if(NOT HAVE_BE64TO AND HAVE_SYS_ENDIAN_H)
    check_symbol_exists(be64toh "sys/endian.h" HAVE_BE64TOH)
  endif()

  check_symbol_exists(bswap_64 "byteswap.h" HAVE_BSWAP_64)

  configure_file(ngtcp2/cmakeconfig.h.in ngtcp2/config.h)
  include_directories("${CMAKE_CURRENT_BINARY_DIR}/ngtcp2") # for config.h
  set(ENABLE_STATIC_LIB ON FORCE BOOL)
  set(ENABLE_SHARED_LIB OFF FORCE BOOL)
  add_subdirectory(ngtcp2/lib EXCLUDE_FROM_ALL)

  target_compile_definitions(ngtcp2_static PRIVATE -DHAVE_CONFIG_H -D_GNU_SOURCE)
endfunction()
