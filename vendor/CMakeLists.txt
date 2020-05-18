add_library(tuntap OBJECT
  libtuntap-master/tuntap.cpp
  libtuntap-master/tuntap_log.cpp)

get_target_property(nlohmann_inc nlohmann_json::nlohmann_json INTERFACE_INCLUDE_DIRECTORIES)
target_include_directories(tuntap PRIVATE ../llarp ../include ${nlohmann_inc})

if(WIN32)
  target_sources(tuntap PRIVATE libtuntap-master/tuntap-windows.c)
else()
  target_sources(tuntap PRIVATE libtuntap-master/tuntap-unix.c)

  if(CMAKE_SYSTEM_NAME MATCHES "Linux|Android")
    target_sources(tuntap PRIVATE libtuntap-master/tuntap-unix-linux.c)
  elseif (${CMAKE_SYSTEM_NAME} MATCHES "OpenBSD")
    target_sources(tuntap PRIVATE libtuntap-master/tuntap-unix-openbsd.c libtuntap-master/tuntap-unix-bsd.c)
  elseif (${CMAKE_SYSTEM_NAME} MATCHES "NetBSD")
    target_sources(tuntap PRIVATE libtuntap-master/tuntap-unix-netbsd.c libtuntap-master/tuntap-unix-bsd.c)
  elseif (${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD|DragonFly")
    target_sources(tuntap PRIVATE libtuntap-master/tuntap-unix-freebsd.c libtuntap-master/tuntap-unix-bsd.c)
  elseif (${CMAKE_SYSTEM_NAME} MATCHES "Darwin|iOS")
    target_sources(tuntap PRIVATE libtuntap-master/tuntap-unix-darwin.c libtuntap-master/tuntap-unix-bsd.c)
  elseif (${CMAKE_SYSTEM_NAME} MATCHES "SunOS")
    target_sources(tuntap PRIVATE libtuntap-master/tuntap-unix-sunos.c)
  else()
    message(FATAL_ERROR "Your operating system - ${CMAKE_SYSTEM_NAME} is not supported yet for libtuntap")
  endif()
endif()
