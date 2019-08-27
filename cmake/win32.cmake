if(NOT WIN32)
  return()
endif()

enable_language(RC)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
if (MSVC OR MSVC_VERSION)
  add_compile_options(/EHca /arch:AVX2 /MD)
  add_definitions(-D_SILENCE_CXX17_OLD_ALLOCATOR_MEMBERS_DEPRECATION_WARNING)
  # If we're building a lokinet for windows, but need to target ancient hardware  
  function(check_working_cxx_atomics64 varname)
    set(OLD_CMAKE_REQUIRED_FLAGS ${CMAKE_REQUIRED_FLAGS})
    set(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} -arch:IA32")
    check_cxx_source_compiles("
  #include <atomic>
  #include <cstdint>
  std::atomic<uint64_t> x (0);
  int main() {
    uint64_t i = x.load(std::memory_order_relaxed);
    return 0;
  }
  " ${varname})
    set(CMAKE_REQUIRED_FLAGS ${OLD_CMAKE_REQUIRED_FLAGS})
  endfunction()

  function(link_libatomic)
    check_working_cxx_atomics64(HAVE_CXX_ATOMICS64_WITHOUT_LIB)

    if(HAVE_CXX_ATOMICS64_WITHOUT_LIB)
      message(STATUS "Have working 64bit atomics")
      return()
    endif()

#    check_library_exists(atomic __atomic_load_8 "" HAVE_CXX_LIBATOMICS64)
#    if (HAVE_CXX_LIBATOMICS64)
#      message(STATUS "Have 64bit atomics via library")
#      list(APPEND CMAKE_REQUIRED_LIBRARIES "atomic")
#      check_working_cxx_atomics64(HAVE_CXX_ATOMICS64_WITH_LIB)
#      if (HAVE_CXX_ATOMICS64_WITH_LIB)
#        message(STATUS "Can link with libatomic")
#        link_libraries(-latomic)
#        return()
#      endif()
#    endif()
    message(FATAL_ERROR "Host compiler must support 64-bit std::atomic! (What do, MSVC doesn't emit __atomic_load_x)")
  endfunction()

  if(EMBEDDED_CFG)
    link_libatomic()
  endif()

  if(${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang" AND "x${CMAKE_CXX_SIMULATE_ID}" STREQUAL "xMSVC")
      add_compile_options(-Wno-nonportable-system-include-path)
  endif()
endif()

if(NOT MSVC_VERSION)
  add_compile_options($<$<COMPILE_LANGUAGE:C>:-Wno-bad-function-cast>)
  add_compile_options($<$<COMPILE_LANGUAGE:C>:-Wno-cast-function-type>)
  # unlike unix where you get a *single* compiler ID string in .comment
  # GNU ld sees fit to merge *all* the .ident sections in object files
  # to .r[o]data section one after the other!
  add_compile_options(-fno-ident -Wa,-mbig-obj)
  link_libraries( -lshlwapi -ldbghelp )
  add_definitions(-DWINVER=0x0500 -D_WIN32_WINNT=0x0500)
  # Wait a minute, if we're not Microsoft C++, nor a Clang paired with Microsoft C++,
  # then the only possible option has to be GNU or a GNU-linked Clang!
  set(FS_LIB stdc++fs)
  # If we're building a lokinet for windows, but need to target ancient hardware  
  function(check_working_cxx_atomics64 varname)
    set(OLD_CMAKE_REQUIRED_FLAGS ${CMAKE_REQUIRED_FLAGS})
    set(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} -march=i486")
    check_cxx_source_compiles("
  #include <atomic>
  #include <cstdint>
  std::atomic<uint64_t> x (0);
  int main() {
    uint64_t i = x.load(std::memory_order_relaxed);
    return 0;
  }
  " ${varname})
    set(CMAKE_REQUIRED_FLAGS ${OLD_CMAKE_REQUIRED_FLAGS})
  endfunction()

  function(link_libatomic)
    check_working_cxx_atomics64(HAVE_CXX_ATOMICS64_WITHOUT_LIB)

    if(HAVE_CXX_ATOMICS64_WITHOUT_LIB)
      message(STATUS "Have working 64bit atomics")
      return()
    endif()

    check_library_exists(atomic __atomic_load_8 "" HAVE_CXX_LIBATOMICS64)
    if (HAVE_CXX_LIBATOMICS64)
      message(STATUS "Have 64bit atomics via library")
      list(APPEND CMAKE_REQUIRED_LIBRARIES "atomic")
      check_working_cxx_atomics64(HAVE_CXX_ATOMICS64_WITH_LIB)
      if (HAVE_CXX_ATOMICS64_WITH_LIB)
        message(STATUS "Can link with libatomic")
        link_libraries(-latomic)
        return()
      endif()
    endif()
    message(FATAL_ERROR "Host compiler must support 64-bit std::atomic!")
  endfunction()
  
  if(EMBEDDED_CFG)
    link_libatomic()
  endif()
endif()

get_filename_component(LIBTUNTAP_IMPL ${TT_ROOT}/tuntap-windows.c ABSOLUTE)
get_filename_component(EV_SRC "llarp/ev/ev_win32.cpp" ABSOLUTE)
add_definitions(-DWIN32_LEAN_AND_MEAN -DWIN32 -DWINVER=0x0500)
set(EXE_LIBS ${STATIC_LIB} ${FS_LIB} ws2_32 iphlpapi)

if(RELEASE_MOTTO)
  add_definitions(-DLLARP_RELEASE_MOTTO="${RELEASE_MOTTO}")
  add_definitions(-DRELEASE_MOTTO=${RELEASE_MOTTO})
endif()

if (NOT STATIC_LINK_RUNTIME AND NOT MSVC)
  message("must ship compiler runtime libraries with this build: libwinpthread-1.dll, libgcc_s_dw2-1.dll, and libstdc++-6.dll")
  message("for release builds, turn on STATIC_LINK_RUNTIME in cmake options")
endif()
