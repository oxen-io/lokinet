get_filename_component(LIBSODIUM_PREFIX "${CMAKE_SOURCE_DIR}/libsodium" ABSOLUTE)
set(LIBSODIUM_URL https://github.com/jedisct1/libsodium/releases/download/1.0.18-RELEASE/libsodium-1.0.18.tar.gz https://download.libsodium.org/libsodium/releases/libsodium-1.0.18.tar.gz)
set(LIBSODIUM_HASH SHA512=17e8638e46d8f6f7d024fe5559eccf2b8baf23e143fadd472a7d29d228b186d86686a5e6920385fe2020729119a5f12f989c3a782afbd05a8db4819bb18666ef)

if(SODIUM_TARBALL_URL)
    # make a build time override of the tarball url so we can fetch it if the original link goes away
    set(LIBSODIUM_URL ${SODIUM_TARBALL_URL})
endif()



include(ExternalProject)
include(ProcessorCount)

if (CMAKE_C_COMPILER_ARG1)
  set(SODIUM_CONFIGURE ./configure --prefix=${LIBSODIUM_PREFIX} --enable-static --disable-shared CC=${CMAKE_C_COMPILER} CPPFLAGS=${CMAKE_C_COMPILER_ARG1})
else()
  if(ANDROID)
    set(SODIUM_CONFIGURE ./configure --prefix=${LIBSODIUM_PREFIX} --host=aarch64-gnu-linux --enable-static --disable-shared CC=${CMAKE_ANDROID_NDK}/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android21-clang)
  else()
    set(SODIUM_CONFIGURE ./configure --prefix=${LIBSODIUM_PREFIX} --enable-static --disable-shared CC=${CMAKE_C_COMPILER})
  endif()
endif()

if(CROSS_TARGET)
  set(SODIUM_CONFIGURE ${SODIUM_CONFIGURE} --host=${CROSS_TARGET})
endif()

set(SODIUM_BUILD make -j${ProcessorCount})
set(SODIUM_INSTALL ${MAKE})

#if(ANDROID)
#  set(SODIUM_CONFIGURE true)
#  set(SODIUM_BUILD ANDROID_NDK_HOME=${CMAKE_ANDROID_NDK} ./dist-build/android-armv8-a.sh)
#  set(SODIUM_INSTALL true)
#endif()
  
file(MAKE_DIRECTORY ${LIBSODIUM_PREFIX}/include)
ExternalProject_Add(libsodium_external
  BUILD_IN_SOURCE ON
  PREFIX ${LIBSODIUM_PREFIX}
  URL ${LIBSODIUM_URL}
  URL_HASH ${LIBSODIUM_HASH}
  CONFIGURE_COMMAND ${SODIUM_CONFIGURE}
  BUILD_COMMAND ${SODIUM_BUILD}
  INSTALL_COMMAND ${SODIUM_INSTALL}
  BUILD_BYPRODUCTS ${LIBSODIUM_PREFIX}/lib/libsodium.a ${LIBSODIUM_PREFIX}/include)

add_library(sodium_vendor STATIC IMPORTED GLOBAL)
add_dependencies(sodium_vendor libsodium_external)
set_target_properties(sodium_vendor PROPERTIES
    IMPORTED_LOCATION ${LIBSODIUM_PREFIX}/lib/libsodium.a
    INTERFACE_INCLUDE_DIRECTORIES ${LIBSODIUM_PREFIX}/include
    )
