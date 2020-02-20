# This is now Win32 exclusive, other platforms can use static_link
if(NOT STATIC_LINK_RUNTIME)
  return()
endif()

if (CMAKE_C_COMPILER_AR)
  add_compile_options(-flto)
  set(CMAKE_AR ${CMAKE_C_COMPILER_AR})
  set(CMAKE_C_ARCHIVE_CREATE "<CMAKE_AR> qcs <TARGET> <LINK_FLAGS> <OBJECTS>")
  set(CMAKE_C_ARCHIVE_FINISH "true")
  set(CMAKE_CXX_ARCHIVE_CREATE "<CMAKE_AR> qcs <TARGET> <LINK_FLAGS> <OBJECTS>")
  set(CMAKE_CXX_ARCHIVE_FINISH "true")
  link_libraries( -flto -static-libstdc++ -static-libgcc -static ${CMAKE_CXX_FLAGS} ${CRYPTO_FLAGS} )
endif()
