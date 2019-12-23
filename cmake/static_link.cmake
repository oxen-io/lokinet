if(NOT STATIC_LINK)
  return()
endif()

# not supported on Solaris - system libraries are not available as archives
# LTO is supported only for native builds
if(SOLARIS)
  link_libraries( -static-libstdc++ -static-libgcc )
  return()
endif()

if(NOT CMAKE_CROSSCOMPILING)
  add_compile_options(-static -flto)
else()
  add_compile_options(-static)
endif()

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  if(APPLE)
    link_libraries( -flto)
  else()
    link_libraries( -static -static-libstdc++ -pthread -flto )
  endif()

  return()
endif()

if(NOT CMAKE_CROSSCOMPILING)
  set(CMAKE_AR "gcc-ar")
  set(CMAKE_C_ARCHIVE_CREATE "<CMAKE_AR> qcs <TARGET> <LINK_FLAGS> <OBJECTS>")
  set(CMAKE_C_ARCHIVE_FINISH "true")
  set(CMAKE_CXX_ARCHIVE_CREATE "<CMAKE_AR> qcs <TARGET> <LINK_FLAGS> <OBJECTS>")
  set(CMAKE_CXX_ARCHIVE_FINISH "true")
  link_libraries( -flto -static-libstdc++ -static-libgcc -static ${CMAKE_CXX_FLAGS} ${CRYPTO_FLAGS} )
else()
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libstdc++ -static-libgcc -static" )
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")
endif()
