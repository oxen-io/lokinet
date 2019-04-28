if(${CMAKE_SYSTEM_NAME} MATCHES "SunOS")
  # check if we have the (saner) emulation of epoll here
  # it's basically linux epoll but with a sane method of
  # dealing with closed file handles that still exist in the
  # epoll set
  #
  # Note that the zombie of Oracle Solaris 2.11.x will NOT have
  # this, the header check is the only method we have to distinguish
  # them. -rick the svr4 guy
  set(SOLARIS ON)
  option(USE_POLL "Revert to using poll(2) event loop (useful if targeting Oracle Solaris)" OFF)
  set(CMAKE_CXX_STANDARD_LIBRARIES "${CMAKE_CXX_STANDARD_LIBRARIES} -lsocket -lnsl")
  add_definitions(-D_POSIX_PTHREAD_SEMANTICS)
  INCLUDE(CheckIncludeFiles)
  CHECK_INCLUDE_FILES(sys/epoll.h SOLARIS_HAVE_EPOLL)
  if (SOLARIS_HAVE_EPOLL AND NOT USE_POLL)
	  message(STATUS "Using fast emulation of Linux epoll(5) on Solaris.")
	  add_definitions(-DSOLARIS_HAVE_EPOLL)
  else()
          set(SOLARIS_HAVE_EPOLL OFF)
	  message(STATUS "Falling back to poll(2)-based event loop.")
  endif()
endif()
