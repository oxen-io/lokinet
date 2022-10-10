if(NOT WIN32)
  return()
endif()
if (NOT STATIC_LINK)
  message(FATAL_ERROR "windows requires static builds (thanks balmer)")
endif()

enable_language(RC)

option(WITH_WINDOWS_32 "build 32 bit windows" OFF)

# unlike unix where you get a *single* compiler ID string in .comment
# GNU ld sees fit to merge *all* the .ident sections in object files
# to .r[o]data section one after the other!
add_compile_options(-fno-ident -Wa,-mbig-obj)
# the minimum windows version, set to 6 rn because supporting older windows is hell
set(_winver 0x0600)
add_definitions(-D_WIN32_WINNT=${_winver})

if(EMBEDDED_CFG)
  link_libatomic()
endif()

set(WINTUN_VERSION 0.14.1 CACHE STRING "wintun version")
set(WINTUN_MIRROR https://www.wintun.net/builds
  CACHE STRING "wintun mirror(s)")
set(WINTUN_SOURCE wintun-${WINTUN_VERSION}.zip)
set(WINTUN_HASH SHA256=07c256185d6ee3652e09fa55c0b673e2624b565e02c4b9091c79ca7d2f24ef51
  CACHE STRING "wintun source hash")

set(WINDIVERT_VERSION 2.2.0-A CACHE STRING "windivert version")
set(WINDIVERT_MIRROR https://reqrypt.org/download
  CACHE STRING "windivert mirror(s)")
set(WINDIVERT_SOURCE WinDivert-${WINDIVERT_VERSION}.zip)
set(WINDIVERT_HASH SHA256=2a7630aac0914746fbc565ac862fa096e3e54233883ac52d17c83107496b7a7f
  CACHE STRING "windivert source hash")

set(WINTUN_URL ${WINTUN_MIRROR}/${WINTUN_SOURCE}
  CACHE STRING "wintun download url")
set(WINDIVERT_URL ${WINDIVERT_MIRROR}/${WINDIVERT_SOURCE}
  CACHE STRING "windivert download url")

message(STATUS "Downloading wintun from ${WINTUN_URL}")
file(DOWNLOAD ${WINTUN_URL} ${CMAKE_BINARY_DIR}/wintun.zip EXPECTED_HASH ${WINTUN_HASH})
message(STATUS "Downloading windivert from ${WINDIVERT_URL}")
file(DOWNLOAD ${WINDIVERT_URL} ${CMAKE_BINARY_DIR}/windivert.zip EXPECTED_HASH ${WINDIVERT_HASH})

execute_process(COMMAND ${CMAKE_COMMAND} -E tar x ${CMAKE_BINARY_DIR}/wintun.zip
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

execute_process(COMMAND ${CMAKE_COMMAND} -E tar x ${CMAKE_BINARY_DIR}/windivert.zip
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
