# dynamic linking does this all the time
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  option(NO_LIBGCC "use libunwind+compiler-rt instead, must already be installed in mingw-w64 sysroot" OFF)
  add_compile_options(-Wno-unused-command-line-argument -Wno-c++11-narrowing)
  add_compile_options($<$<COMPILE_LANGUAGE:C>:-Wno-bad-function-cast>)
  if (NO_LIBGCC)
    set(CMAKE_CXX_STANDARD_LIBRARIES "-lunwind -lpsapi ${CMAKE_CXX_STANDARD_LIBRARIES}")
    set(CMAKE_C_STANDARD_LIBRARIES "-lunwind -lpsapi ${CMAKE_C_STANDARD_LIBRARIES}")
  endif(NO_LIBGCC)
else()
  # found it. this is GNU only
  add_compile_options(-Wno-cast-function-type)
endif()
