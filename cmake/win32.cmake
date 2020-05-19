if(NOT WIN32)
  return()
endif()

enable_language(RC)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

if(NOT MSVC_VERSION)
  add_compile_options($<$<COMPILE_LANGUAGE:C>:-Wno-bad-function-cast>)
  add_compile_options($<$<COMPILE_LANGUAGE:C>:-Wno-cast-function-type>)
  # unlike unix where you get a *single* compiler ID string in .comment
  # GNU ld sees fit to merge *all* the .ident sections in object files
  # to .r[o]data section one after the other!
  add_compile_options(-fno-ident -Wa,-mbig-obj)
  link_libraries( -lws2_32 -lshlwapi -ldbghelp -luser32 -liphlpapi -lpsapi -luserenv )
  add_definitions(-DWINVER=0x0500 -D_WIN32_WINNT=0x0500)
endif()

if(EMBEDDED_CFG)
  link_libatomic()
endif()

add_definitions(-DWIN32_LEAN_AND_MEAN -DWIN32)

if (NOT STATIC_LINK AND NOT MSVC)
  message("must ship compiler runtime libraries with this build: libwinpthread-1.dll, libgcc_s_dw2-1.dll, and libstdc++-6.dll")
  message("for release builds, turn on STATIC_LINK in cmake options")
endif()

# win32 is the last platform for which we grab libuv manually
# if you want to use the included submodule do
# cmake .. -G Ninja -DLIBUV_ROOT=../external/libuv.
# otherwise grab mine (github.com/despair86/libuv.git) if you need to run on older hardware
# and clone to win32-setup/libuv
# then
# cmake .. -G Ninja -DLIBUV_ROOT=../win32-setup/libuv
# it is literally upward compatible with current windows 10
if (STATIC_LINK)
  set(LIBUV_USE_STATIC ON)
  if (WOW64_CROSS_COMPILE)
    link_libraries( -static-libstdc++ -static-libgcc -static -Wl,--image-base=0x10000,--large-address-aware,--dynamicbase,--pic-executable,-e,_mainCRTStartup,--subsystem,console:5.00 )
  else()
    link_libraries( -static-libstdc++ -static-libgcc -static -Wl,--image-base=0x10000,--dynamicbase,--pic-executable,-e,_mainCRTStartup )
  endif()
endif()

if(LIBUV_ROOT)
  add_subdirectory(${LIBUV_ROOT})
  set(LIBUV_INCLUDE_DIRS ${LIBUV_ROOT}/include)
  set(LIBUV_LIBRARY uv_a)
  add_definitions(-D_LARGEFILE_SOURCE)
  add_definitions(-D_FILE_OFFSET_BITS=64)
elseif(NOT LIBUV_IN_SOURCE)
  find_package(LibUV 1.28.0 REQUIRED)
endif()

include_directories(${LIBUV_INCLUDE_DIRS})
