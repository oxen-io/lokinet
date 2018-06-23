# libtuntap CMakeLists.txt
# ========================

project(libtuntap)
cmake_minimum_required(VERSION 2.8)

# CMake global options
# --------------------
option(ENABLE_REGRESS "Enable the regression tests" OFF)
option(ENABLE_CXX "Enable the C++ wrapper library" OFF)
option(ENABLE_PYTHON "Enable the Python wrapper library" OFF)

if(ENABLE_PYTHON AND NOT ENABLE_CXX)
    set(ENABLE_CXX ON)
    message(WARNING "ENABLE_CXX also set to ON")
endif()

# CMake Configuration
# -------------------
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/lib)
set(CMAKE_INSTALL_PREFIX "/usr/local/")
set(${CMAKE_SYSTEM_NAME} True)

# Global CPP definitions
# ----------------------
add_definitions(-D${CMAKE_SYSTEM_NAME})

# Portable source files
# ---------------------
set(SOURCES_LIST
    tuntap.c
    tuntap_log.c
)

# OS families specific things
# ---------------------------
if(UNIX)
    # Unix specific include directories
    # ---------------------------------
    include_directories(
        "${CMAKE_CURRENT_SOURCE_DIR}"
        /usr/include/
        /usr/local/include
    )

    # Unix specific definitions
    # -------------------------
    add_definitions(-DUnix)

    # Unix specific source files
    # --------------------------
    set(SOURCES_LIST ${SOURCES_LIST} tuntap-unix.c )
endif(UNIX)

if(Windows)
    # Windows specific definitions
    # ----------------------------
    add_definitions(-DWindows)

    # Windows specific source files
    # -----------------------------
    set(SOURCES_LIST ${SOURCES_LIST} tuntap-windows.c )
endif(Windows)

# OS specific things
# ------------------
if(UNIX)
    if(Linux)
        set(CMAKE_INSTALL_PREFIX "/usr/")
        add_definitions(-D_GNU_SOURCE)
        set(SOURCES_LIST ${SOURCES_LIST} tuntap-unix-linux.c)
    elseif (OpenBSD)
        set(SOURCES_LIST ${SOURCES_LIST} tuntap-unix-openbsd.c)
        set(SOURCES_LIST ${SOURCES_LIST} tuntap-unix-bsd.c)
    elseif (NetBSD)
        set(SOURCES_LIST ${SOURCES_LIST} tuntap-unix-netbsd.c)
        set(SOURCES_LIST ${SOURCES_LIST} tuntap-unix-bsd.c)
    elseif (FreeBSD)
        set(SOURCES_LIST ${SOURCES_LIST} tuntap-unix-freebsd.c)
        set(SOURCES_LIST ${SOURCES_LIST} tuntap-unix-bsd.c)
    elseif (Darwin)
        set(SOURCES_LIST ${SOURCES_LIST} tuntap-unix-darwin.c)
        set(SOURCES_LIST ${SOURCES_LIST} tuntap-unix-bsd.c)
    elseif (DragonFly)
        set(SOURCES_LIST ${SOURCES_LIST} tuntap-unix-freebsd.c)
        set(SOURCES_LIST ${SOURCES_LIST} tuntap-unix-bsd.c)
    else()
        message(FATAL_ERROR "Your operating system is not supported yet")
    endif()
endif(UNIX)

# Library definition
# ------------------
add_library(tuntap SHARED ${SOURCES_LIST} tuntap.h)
set_target_properties(tuntap PROPERTIES VERSION 2.1)

add_library(tuntap-static STATIC ${SOURCES_LIST})
set_target_properties(tuntap-static PROPERTIES OUTPUT_NAME "tuntap")
if(UNIX)
    set_target_properties(tuntap-static PROPERTIES PREFIX "lib")
endif(UNIX)

if(Windows)
    target_link_libraries(tuntap Ws2_32.lib)
    target_link_libraries(tuntap-static Ws2_32.lib)
endif(Windows)

# C++ Binding definition
# ----------------------
if(ENABLE_CXX)
    include(bindings/cpp/CMakeLists.txt)
endif(ENABLE_CXX)

# Python Binding definition
# -------------------------
if(ENABLE_PYTHON)
    include(bindings/python/CMakeLists.txt)
endif (ENABLE_PYTHON)

# Install rules
# -------------
if(UNIX)
	install(TARGETS tuntap DESTINATION lib)
	install(TARGETS tuntap-static DESTINATION lib)
	install(FILES tuntap.h DESTINATION include)
	target_include_directories(tuntap INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
endif(UNIX)

include(CMakeLists.txt.local OPTIONAL)

# Tests rules
# -----------
if (ENABLE_REGRESS)
    include(regress/CMakeLists.txt)
endif (ENABLE_REGRESS)

