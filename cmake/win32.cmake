enable_language(RC)
set(CMAKE_CXX_STANDARD_LIBRARIES "${CMAKE_CXX_STANDARD_LIBRARIES} -lshlwapi")

if(NOT MSVC_VERSION)
  add_compile_options($<$<COMPILE_LANGUAGE:C>:-Wno-bad-function-cast>)
  add_compile_options($<$<COMPILE_LANGUAGE:C>:-Wno-cast-function-type>)
  # unlike unix where you get a *single* compiler ID string in .comment
  # GNU ld sees fit to merge *all* the .ident sections in object files
  # to .r[o]data section one after the other!
  add_compile_options(-fno-ident)
  set(FS_LIB stdc++fs)
endif()

get_filename_component(LIBTUNTAP_IMPL ${TT_ROOT}/tuntap-windows.c ABSOLUTE)
get_filename_component(EV_SRC "llarp/ev/ev_win32.cpp" ABSOLUTE)
add_definitions(-DWIN32_LEAN_AND_MEAN -DWIN32 -DWINVER=0x0500 -D_WIN32_WINNT=0x0500)
set(EXE_LIBS ${STATIC_LIB} ${FS_LIB} ws2_32 iphlpapi)

if(RELEASE_MOTTO)
  add_definitions(-DLLARP_RELEASE_MOTTO="${RELEASE_MOTTO}")
  add_definitions(-DRELEASE_MOTTO=${RELEASE_MOTTO})
endif()

if (NOT STATIC_LINK_RUNTIME AND NOT MSVC)
  message("must ship compiler runtime libraries with this build: libwinpthread-1.dll, libgcc_s_dw2-1.dll, and libstdc++-6.dll")
  message("for release builds, turn on STATIC_LINK_RUNTIME in cmake options")
endif()