set(LIBSODIUM_PREFIX ${CMAKE_BINARY_DIR}/libsodium)
set(LIBSODIUM_SRC ${LIBSODIUM_PREFIX}/libsodium-1.0.18)
set(LIBSODIUM_TARBALL ${LIBSODIUM_PREFIX}/libsodium-1.0.18.tar.gz)
set(LIBSODIUM_URL https://github.com/jedisct1/libsodium/releases/download/1.0.18-RELEASE/libsodium-1.0.18.tar.gz)
if(SODIUM_TARBALL_URL)
    # make a build time override of the tarball url so we can fetch it if the original link goes away
    set(LIBSODIUM_URL ${SODIUM_TARBALL_URL})
endif()
set(SODIUM_PRETEND_TO_BE_CONFIGURED ON)
file(DOWNLOAD
    ${LIBSODIUM_URL}
    ${LIBSODIUM_TARBALL}
    EXPECTED_HASH SHA512=17e8638e46d8f6f7d024fe5559eccf2b8baf23e143fadd472a7d29d228b186d86686a5e6920385fe2020729119a5f12f989c3a782afbd05a8db4819bb18666ef
    SHOW_PROGRESS)

execute_process(COMMAND tar -xzf ${LIBSODIUM_TARBALL} -C ${LIBSODIUM_PREFIX})
if(WIN32)
  message("patch -p0 -d ${LIBSODIUM_SRC} < ${CMAKE_SOURCE_DIR}/llarp/win32/libsodium-1.0.18-win32.patch")
  execute_process(COMMAND "patch -p0 -d ${LIBSODIUM_SRC} < ${CMAKE_SOURCE_DIR}/llarp/win32/libsodium-1.0.18-win32.patch")
endif()
file(GLOB_RECURSE sodium_sources
    ${LIBSODIUM_SRC}/src/libsodium/*.c
    ${LIBSODIUM_SRC}/src/libsodium/*.h
    )
add_library(sodium_vendor ${sodium_sources})

set_target_properties(sodium_vendor
    PROPERTIES
        C_STANDARD 99
)

target_include_directories(sodium_vendor
    PUBLIC
        ${LIBSODIUM_SRC}/src/libsodium/include
    PRIVATE
        ${LIBSODIUM_SRC}/src/libsodium/include/sodium
)

target_compile_definitions(sodium_vendor
    PUBLIC
        $<$<NOT:$<BOOL:${BUILD_SHARED_LIBS}>>:SODIUM_STATIC>
        $<$<BOOL:${SODIUM_MINIMAL}>:SODIUM_LIBRARY_MINIMAL>
    PRIVATE
        $<$<BOOL:${BUILD_SHARED_LIBS}>:SODIUM_DLL_EXPORT>
        $<$<BOOL:${SODIUM_ENABLE_BLOCKING_RANDOM}>:USE_BLOCKING_RANDOM>
        $<$<BOOL:${SODIUM_MINIMAL}>:MINIMAL>
        $<$<BOOL:${SODIUM_PRETEND_TO_BE_CONFIGURED}>:CONFIGURED>
)

# Variables that need to be exported to version.h.in
set(VERSION_ORIG "${VERSION}") # an included module sets things in the calling scope :(
set(VERSION 1.0.18)
set(SODIUM_LIBRARY_VERSION_MAJOR 10)
set(SODIUM_LIBRARY_VERSION_MINOR 3)

configure_file(
  ${LIBSODIUM_SRC}/src/libsodium/include/sodium/version.h.in
  ${LIBSODIUM_SRC}/src/libsodium/include/sodium/version.h
)

target_sources(sodium_vendor PRIVATE ${LIBSODIUM_SRC}/src/libsodium/include/sodium/version.h)

set(VERSION "${VERSION_ORIG}")
