set(CMAKE_SYSTEM_NAME Windows)
set(TOOLCHAIN_PREFIX x86_64-w64-mingw32)
set(WIN64_CROSS_COMPILE ON)
set(CROSS_TARGET x86_64-w64-mingw32)

set(TOOLCHAIN_PATHS
 /usr/${TOOLCHAIN_PREFIX}
 /usr/local/opt/mingw-w64/toolchain-x86_64
 /usr/local/opt/mingw-w64/toolchain-x86_64/x86_64-w64-mingw32
 /opt/mingw64
 /home/$ENV{USER}/mingw32
 /home/$ENV{USER}/mingw64
 /home/$ENV{USER}/mingw64/${TOOLCHAIN_PREFIX}
 /home/$ENV{USER}/mingw32/${TOOLCHAIN_PREFIX})

include("${CMAKE_CURRENT_LIST_DIR}/mingw_core.cmake")
