set(LIBCURL_PREFIX ${CMAKE_BINARY_DIR}/libcurl)
set(LIBCURL_URL https://github.com/curl/curl/releases/download/curl-7_67_0/curl-7.67.0.tar.xz)
set(LIBCURL_HASH SHA256=f5d2e7320379338c3952dcc7566a140abb49edb575f9f99272455785c40e536c)

if(CURL_TARBALL_URL)
    # make a build time override of the tarball url so we can fetch it if the original link goes away
    set(LIBCURL_URL ${CURL_TARBALL_URL})
endif()


file(MAKE_DIRECTORY ${LIBCURL_PREFIX}/include)

include(ExternalProject)
include(ProcessorCount)

set(CURL_CONFIGURE_FLAGS_COMMON --prefix=${LIBCURL_PREFIX} --without-ssl --without-nss --without-ca-bundle --without-libidn2 --without-zlib --without-librtmp --without-ca-fallback --without-ca-path --enable-static --disable-shared CC=${CMAKE_C_COMPILER})

if (CMAKE_C_COMPILER_ARG1)
  set(CURL_CONFIGURE_FLAGS_COMMON ${CURL_CONFIGURE_FLAGS_COMMON} CPPFLAGS=${CMAKE_C_COMPILER_ARG1})
endif()

set(CURL_CONFIGURE ./configure ${CURL_CONFIGURE_FLAGS_COMMON})


if (CROSS_TARGET)
    set(CURL_CONFIGURE ${CURL_CONFIGURE} --target=${CROSS_TARGET} --host=${CROSS_TARGET})
endif()


ExternalProject_Add(libcurl_external
    BUILD_IN_SOURCE ON
    PREFIX ${LIBCURL_PREFIX}
    URL ${LIBCURL_URL}
    URL_HASH ${LIBCURL_HASH}
    CONFIGURE_COMMAND ${CURL_CONFIGURE}
    BUILD_COMMAND make -j${PROCESSOR_COUNT}
    INSTALL_COMMAND ${MAKE}
    BUILD_BYPRODUCTS ${LIBCURL_PREFIX}/lib/libcurl.a ${LIBCURL_PREFIX}/include
)

add_library(curl_vendor STATIC IMPORTED GLOBAL)
add_dependencies(curl_vendor curl_external)
set_target_properties(curl_vendor PROPERTIES
    IMPORTED_LOCATION ${LIBCURL_PREFIX}/lib/libcurl.a
    INTERFACE_INCLUDE_DIRECTORIES ${LIBCURL_PREFIX}/include
)
