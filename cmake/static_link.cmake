if(NOT STATIC_LINK)
  return()
endif()

if(NOT CMAKE_CROSSCOMPILING)
  add_compile_options(-static -flto)
else()
  add_compile_options(-static)
endif()

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  link_libraries( -flto)
endif()
