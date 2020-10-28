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
  # zmq requires windows xp or higher
  add_definitions(-DWINVER=0x0501 -D_WIN32_WINNT=0x0501)
endif()

if(EMBEDDED_CFG)
  link_libatomic()
endif()

add_definitions(-DWIN32_LEAN_AND_MEAN -DWIN32)

if (NOT STATIC_LINK AND NOT MSVC)
  message("must ship compiler runtime libraries with this build: libwinpthread-1.dll, libgcc_s_dw2-1.dll, and libstdc++-6.dll")
  message("for release builds, turn on STATIC_LINK in cmake options")
endif()

if (STATIC_LINK)
  set(LIBUV_USE_STATIC ON)
  if (WOW64_CROSS_COMPILE)
    link_libraries( -static-libstdc++ -static-libgcc -static -Wl,--image-base=0x10000,--large-address-aware,--dynamicbase,--pic-executable,-e,_mainCRTStartup,--subsystem,console:5.00 )
  else()
    link_libraries( -static-libstdc++ -static-libgcc -static -Wl,--image-base=0x10000,--dynamicbase,--pic-executable,-e,mainCRTStartup )
  endif()
endif()

# win32 is the last platform for which we grab libuv manually.
# If you want to run on older hardware try github.com/despair86/libuv.git and then:
#     cmake .. -G Ninja -DLIBUV_ROOT=/path/to/libuv
# Otherwise we'll try either a system one (if not under BUILD_STATIC_DEPS) or else use the submodule
# in external/libuv.
add_library(libuv INTERFACE)
if(NOT LIBUV_ROOT AND NOT BUILD_STATIC_DEPS)
  find_package(LibUV 1.28.0)
endif()

if(LibUV_FOUND)
  message(STATUS "using system libuv")
  target_link_libraries(libuv INTERFACE ${LIBUV_LIBRARIES})
  target_include_directories(libuv INTERFACE ${LIBUV_INCLUDE_DIRS})
else()
  if(LIBUV_ROOT)
    add_subdirectory(${LIBUV_ROOT})
  else()
    add_subdirectory(${PROJECT_SOURCE_DIR}/external/libuv)
  endif()
  target_link_libraries(libuv INTERFACE uv_a)
  target_compile_definitions(libuv INTERFACE _LARGEFILE_SOURCE _FILE_OFFSET_BITS=64)
endif()
