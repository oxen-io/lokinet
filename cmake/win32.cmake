if(NOT WIN32)
  return()
endif()

enable_language(RC)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

if(NOT MSVC_VERSION)
  add_compile_options($<$<COMPILE_LANGUAGE:C>:-Wno-bad-function-cast>)
  add_compile_options($<$<COMPILE_LANGUAGE:C>:-Wno-cast-function-type>)
  add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-fpermissive>)
  # unlike unix where you get a *single* compiler ID string in .comment
  # GNU ld sees fit to merge *all* the .ident sections in object files
  # to .r[o]data section one after the other!
  add_compile_options(-fno-ident -Wa,-mbig-obj)
  link_libraries( -lws2_32 -lshlwapi -ldbghelp -luser32 -liphlpapi -lpsapi -luserenv)
  # the minimum windows version, set to 6 rn because supporting older windows is hell
  set(_winver 0x0600)
  add_definitions(-DWINVER=${_winver} -D_WIN32_WINNT=${_winver})
endif()

if(EMBEDDED_CFG)
  link_libatomic()
endif()

add_definitions(-DWIN32_LEAN_AND_MEAN -DWIN32)

if (NOT STATIC_LINK AND NOT MSVC)
  message("must ship compiler runtime libraries with this build: libwinpthread-1.dll, libgcc_s_dw2-1.dll, and libstdc++-6.dll")
  message("for release builds, turn on STATIC_LINK in cmake options")
endif()
