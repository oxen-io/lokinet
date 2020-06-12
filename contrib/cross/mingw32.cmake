set(CMAKE_SYSTEM_NAME Windows)
set(TOOLCHAIN_PREFIX i686-w64-mingw32)
set(WOW64_CROSS_COMPILE ON)
set(CROSS_TARGET i686-w64-mingw32)

set(TOOLCHAIN_PATHS
  /usr/${TOOLCHAIN_PREFIX}
  /usr/local/opt/mingw-w64/toolchain-i686
  /usr/local/opt/mingw-w64/toolchain-i686/i686-w64-mingw32
  /opt/mingw32
  /home/$ENV{USER}/mingw32
  /home/$ENV{USER}/mingw32/${TOOLCHAIN_PREFIX}
)

include("${CMAKE_CURRENT_LIST_DIR}/mingw_core.cmake")
