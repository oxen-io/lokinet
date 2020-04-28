get_filename_component(LIBSODIUM_PREFIX "${CMAKE_BINARY_DIR}/libsodium" ABSOLUTE)
set(LIBSODIUM_URL https://github.com/jedisct1/libsodium/releases/download/1.0.18-RELEASE/libsodium-1.0.18.tar.gz https://download.libsodium.org/libsodium/releases/libsodium-1.0.18.tar.gz)
set(LIBSODIUM_HASH SHA512=17e8638e46d8f6f7d024fe5559eccf2b8baf23e143fadd472a7d29d228b186d86686a5e6920385fe2020729119a5f12f989c3a782afbd05a8db4819bb18666ef)

if(SODIUM_TARBALL_URL)
    # make a build time override of the tarball url so we can fetch it if the original link goes away
    set(LIBSODIUM_URL ${SODIUM_TARBALL_URL})
endif()



include(ExternalProject)
include(ProcessorCount)

if(ANDROID)
  # TODO other android targets
  if("${CMAKE_ANDROID_ARCH_ABI}" STREQUAL "arm64-v8a")  
    set(android_host_tuple aarch64-gnu-linux)
    set(android_cflags -Os)
  elseif("${CMAKE_ANDROID_ARCH_ABI}" STREQUAL "armeabi-v7a")
    set(android_host_tuple armv7-none-linux-androideabi2)
    set(android_cflags -Os -mfloat-abi=softfp -mfpu=vfpv3-d16 -mthumb -marm -march=armv7-a)
  else()
    message(FATAL "cannot determine android host tuple from CMAKE_ANDROID_ARCH_ABI = ${CMAKE_ANDROID_ARCH_ABI}")
  endif()
  set(SODIUM_CONFIGURE ./configure --prefix=${LIBSODIUM_PREFIX} --host=${android_host_tuple} --enable-static --disable-shared CC=${CMAKE_CXX_ANDROID_TOOLCHAIN_PREFIX}clang${CMAKE_CXX_ANDROID_TOOLCHAIN_SUFFIX} CFLAGS=${android_cflags})
else()
  set(SODIUM_CONFIGURE ./configure --prefix=${LIBSODIUM_PREFIX} --enable-static --disable-shared CC=${CMAKE_C_COMPILER})
endif()

if(CROSS_TARGET)
  set(SODIUM_CONFIGURE ${SODIUM_CONFIGURE} --host=${CROSS_TARGET})
endif()

set(SODIUM_BUILD make -j${ProcessorCount})
set(SODIUM_INSTALL ${MAKE})

file(MAKE_DIRECTORY ${LIBSODIUM_PREFIX}/include)
message("configure libsodium using " ${SODIUM_CONFIGURE})
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
