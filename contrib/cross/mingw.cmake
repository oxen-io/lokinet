set(CMAKE_SYSTEM_NAME Windows)
set(TOOLCHAIN_PREFIX x86_64-w64-mingw32)
set(TOOLCHAIN_SUFFIX "")

add_definitions("-DWINNT_CROSS_COMPILE")

# target environment on the build host system
# second one is for non-root installs
set(CMAKE_FIND_ROOT_PATH /usr/${TOOLCHAIN_PREFIX} /usr/local/opt/mingw-w64/toolchain-x86_64/ /opt/mingw64 /home/$ENV{USER}/mingw32 /home/$ENV{USER}/mingw64 /home/$ENV{USER}/mingw64/${TOOLCHAIN_PREFIX} /home/$ENV{USER}/mingw32/${TOOLCHAIN_PREFIX})

# modify default behavior of FIND_XXX() commands
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# cross compilers to use
if($ENV{COMPILER} MATCHES "clang")
    set(USING_CLANG ON)
    set(CMAKE_C_COMPILER ${TOOLCHAIN_PREFIX}-clang)
    set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}-clang++)
else()
    set(CMAKE_C_COMPILER ${TOOLCHAIN_PREFIX}-gcc${TOOLCHAIN_SUFFIX})
    set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}-g++${TOOLCHAIN_SUFFIX})
    add_compile_options("-Wa,-mbig-obj")
endif()

set(CMAKE_RC_COMPILER ${TOOLCHAIN_PREFIX}-windres)
set(WIN64_CROSS_COMPILE ON)
