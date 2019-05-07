add_definitions(-DUNIX)
add_definitions(-DPOSIX)

if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
  get_filename_component(LIBTUNTAP_IMPL ${TT_ROOT}/tuntap-unix-linux.c ABSOLUTE)
  get_filename_component(EV_SRC "llarp/ev/ev_epoll.cpp" ABSOLUTE)
elseif(${CMAKE_SYSTEM_NAME} MATCHES "Android")
  get_filename_component(LIBTUNTAP_IMPL ${TT_ROOT}/tuntap-unix-linux.c ABSOLUTE)
  get_filename_component(EV_SRC "llarp/ev/ev_epoll.cpp" ABSOLUTE)
elseif (${CMAKE_SYSTEM_NAME} MATCHES "OpenBSD")
  set(LIBTUNTAP_IMPL ${TT_ROOT}/tuntap-unix-openbsd.c ${TT_ROOT}/tuntap-unix-bsd.c)
  get_filename_component(EV_SRC "llarp/ev/ev_kqueue.cpp" ABSOLUTE)
elseif (${CMAKE_SYSTEM_NAME} MATCHES "NetBSD")
  set(LIBTUNTAP_IMPL ${TT_ROOT}/tuntap-unix-netbsd.c ${TT_ROOT}/tuntap-unix-bsd.c)
  get_filename_component(EV_SRC "llarp/ev/ev_kqueue.cpp" ABSOLUTE)
elseif (${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD" OR ${CMAKE_SYSTEM_NAME} MATCHES "DragonFly")
  set(LIBTUNTAP_IMPL ${TT_ROOT}/tuntap-unix-freebsd.c ${TT_ROOT}/tuntap-unix-bsd.c)
  get_filename_component(EV_SRC "llarp/ev/ev_kqueue.cpp" ABSOLUTE)
elseif (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  set(LIBTUNTAP_IMPL ${TT_ROOT}/tuntap-unix-darwin.c ${TT_ROOT}/tuntap-unix-bsd.c)
  get_filename_component(EV_SRC "llarp/ev/ev_kqueue.cpp" ABSOLUTE)
elseif (${CMAKE_SYSTEM_NAME} MATCHES "SunOS")
  set(LIBTUNTAP_IMPL ${TT_ROOT}/tuntap-unix-sunos.c)
  if (SOLARIS_HAVE_EPOLL)
  get_filename_component(EV_SRC "llarp/ev/ev_epoll.cpp" ABSOLUTE)
  else()
  get_filename_component(EV_SRC "llarp/ev/ev_sun.cpp" ABSOLUTE)
  endif()
else()
  message(FATAL_ERROR "Your operating system is not supported yet")
endif()

set(UTIL_EXTRA_LIBS absl::optional absl::variant absl::strings cppbackport)
set(EXE_LIBS ${STATIC_LIB} cppbackport libutp)
set(ABYSS_EXTRA_LIBS Threads::Threads)

if(RELEASE_MOTTO)
  add_definitions(-DLLARP_RELEASE_MOTTO="${RELEASE_MOTTO}")
endif()
