set(LIBSODIUM_PREFIX ${CMAKE_BINARY_DIR}/libsodium)
set(LIBSODIUM_URL https://github.com/jedisct1/libsodium/releases/download/1.0.18-RELEASE/libsodium-1.0.18.tar.gz https://download.libsodium.org/libsodium/releases/libsodium-1.0.18.tar.gz)
set(LIBSODIUM_HASH SHA512=17e8638e46d8f6f7d024fe5559eccf2b8baf23e143fadd472a7d29d228b186d86686a5e6920385fe2020729119a5f12f989c3a782afbd05a8db4819bb18666ef)

if(SODIUM_TARBALL_URL)
    # make a build time override of the tarball url so we can fetch it if the original link goes away
    set(LIBSODIUM_URL ${SODIUM_TARBALL_URL})
endif()


file(MAKE_DIRECTORY ${LIBSODIUM_PREFIX}/include)

include(ExternalProject)
include(ProcessorCount)
ProcessorCount(PROCESSOR_COUNT)
if(PROCESSOR_COUNT EQUAL 0)
    set(PROCESSOR_COUNT 1)
endif()

set(sodium_cc ${CMAKE_C_COMPILER})
if(CCACHE_PROGRAM)
  set(sodium_cc "${CCACHE_PROGRAM} ${sodium_cc}")
endif()
set(SODIUM_CONFIGURE ./configure --prefix=${LIBSODIUM_PREFIX} --enable-static --disable-shared --with-pic --quiet CC=${sodium_cc})
if (CMAKE_C_COMPILER_ARG1)
  set(SODIUM_CONFIGURE ${SODIUM_CONFIGURE} CPPFLAGS=${CMAKE_C_COMPILER_ARG1})
endif()

if (CROSS_TARGET)
    set(SODIUM_CONFIGURE ${SODIUM_CONFIGURE} --target=${CROSS_TARGET} --host=${CROSS_TARGET})
endif()


ExternalProject_Add(libsodium_external
    BUILD_IN_SOURCE ON
    PREFIX ${LIBSODIUM_PREFIX}
    URL ${LIBSODIUM_URL}
    URL_HASH ${LIBSODIUM_HASH}
    CONFIGURE_COMMAND ${SODIUM_CONFIGURE}
    PATCH_COMMAND patch -p1 -d <SOURCE_DIR> < ${CMAKE_SOURCE_DIR}/win32-setup/libsodium-1.0.18-win32.patch
    BUILD_COMMAND make -j${PROCESSOR_COUNT}
    INSTALL_COMMAND ${MAKE}
    BUILD_BYPRODUCTS ${LIBSODIUM_PREFIX}/lib/libsodium.a ${LIBSODIUM_PREFIX}/include
    )

add_library(sodium_vendor STATIC IMPORTED GLOBAL)
add_dependencies(sodium_vendor libsodium_external)
set_target_properties(sodium_vendor PROPERTIES
    IMPORTED_LOCATION ${LIBSODIUM_PREFIX}/lib/libsodium.a
    INTERFACE_INCLUDE_DIRECTORIES ${LIBSODIUM_PREFIX}/include
    )
