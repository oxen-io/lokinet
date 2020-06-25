# cmake bits to do a full static build, downloading and building all dependencies.

# Most of these are CACHE STRINGs so that you can override them using -DWHATEVER during cmake
# invocation to override.

set(LOCAL_MIRROR "" CACHE STRING "local mirror path/URL for lib downloads")

set(OPENSSL_VERSION 1.1.1g CACHE STRING "openssl version")
set(OPENSSL_MIRROR ${LOCAL_MIRROR} https://www.openssl.org/source CACHE STRING "openssl download mirror(s)")
set(OPENSSL_SOURCE openssl-${OPENSSL_VERSION}.tar.gz)
set(OPENSSL_HASH SHA256=ddb04774f1e32f0c49751e21b67216ac87852ceb056b75209af2443400636d46
    CACHE STRING "openssl source hash")

set(EXPAT_VERSION 2.2.9 CACHE STRING "expat version")
string(REPLACE "." "_" EXPAT_TAG "R_${EXPAT_VERSION}")
set(EXPAT_MIRROR ${LOCAL_MIRROR} https://github.com/libexpat/libexpat/releases/download/${EXPAT_TAG}
    CACHE STRING "expat download mirror(s)")
set(EXPAT_SOURCE expat-${EXPAT_VERSION}.tar.xz)
set(EXPAT_HASH SHA512=e082874efcc4b00709e2c0192c88fb15dfc4f33fc3a2b09e619b010ea93baaf7e7572683f738463db0ce2350cab3de48a0c38af6b74d1c4f5a9e311f499edab0
    CACHE STRING "expat source hash")

set(UNBOUND_VERSION 1.10.1 CACHE STRING "unbound version")
set(UNBOUND_MIRROR ${LOCAL_MIRROR} https://nlnetlabs.nl/downloads/unbound CACHE STRING "unbound download mirror(s)")
set(UNBOUND_SOURCE unbound-${UNBOUND_VERSION}.tar.gz)
set(UNBOUND_HASH SHA256=b73677c21a71cf92f15cc8cfe76a3d875e40f65b6150081c39620b286582d536
    CACHE STRING "unbound source hash")

set(SQLITE3_VERSION 3320200 CACHE STRING "sqlite3 version")
set(SQLITE3_MIRROR ${LOCAL_MIRROR} https://www.sqlite.org/2020
    CACHE STRING "sqlite3 download mirror(s)")
set(SQLITE3_SOURCE sqlite-autoconf-${SQLITE3_VERSION}.tar.gz)
set(SQLITE3_HASH SHA512=5b551a1366ce4fd5dfaa687e5021194d34315935b26dd7d71f8abc9935d03c3caea323263a8330fb42038c487cd399e95de68e451cc26d573f852f219c00a02f
    CACHE STRING "sqlite3 source hash")

set(SODIUM_VERSION 1.0.18 CACHE STRING "libsodium version")
set(SODIUM_MIRROR ${LOCAL_MIRROR}
  https://download.libsodium.org/libsodium/releases
  https://github.com/jedisct1/libsodium/releases/download/${SODIUM_VERSION}-RELEASE
  CACHE STRING "libsodium mirror(s)")
set(SODIUM_SOURCE libsodium-${SODIUM_VERSION}.tar.gz)
set(SODIUM_HASH SHA512=17e8638e46d8f6f7d024fe5559eccf2b8baf23e143fadd472a7d29d228b186d86686a5e6920385fe2020729119a5f12f989c3a782afbd05a8db4819bb18666ef
  CACHE STRING "libsodium source hash")

set(ZMQ_VERSION 4.3.2 CACHE STRING "libzmq version")
set(ZMQ_MIRROR ${LOCAL_MIRROR} https://github.com/zeromq/libzmq/releases/download/v${ZMQ_VERSION}
    CACHE STRING "libzmq mirror(s)")
set(ZMQ_SOURCE zeromq-${ZMQ_VERSION}.tar.gz)
set(ZMQ_HASH SHA512=b6251641e884181db9e6b0b705cced7ea4038d404bdae812ff47bdd0eed12510b6af6846b85cb96898e253ccbac71eca7fe588673300ddb9c3109c973250c8e4
    CACHE STRING "libzmq source hash")



include(ExternalProject)

set(DEPS_DESTDIR ${CMAKE_BINARY_DIR}/static-deps)
set(DEPS_SOURCEDIR ${CMAKE_BINARY_DIR}/static-deps-sources)

include_directories(BEFORE SYSTEM ${DEPS_DESTDIR}/include)

file(MAKE_DIRECTORY ${DEPS_DESTDIR}/include)

set(deps_cc "${CMAKE_C_COMPILER}")
set(deps_cxx "${CMAKE_CXX_COMPILER}")
if(CMAKE_C_COMPILER_LAUNCHER)
  set(deps_cc "${CMAKE_C_COMPILER_LAUNCHER} ${deps_cc}")
endif()
if(CMAKE_CXX_COMPILER_LAUNCHER)
  set(deps_cxx "${CMAKE_CXX_COMPILER_LAUNCHER} ${deps_cxx}")
endif()

function(expand_urls output source_file)
  set(expanded)
  foreach(mirror ${ARGN})
    list(APPEND expanded "${mirror}/${source_file}")
  endforeach()
  set(${output} "${expanded}" PARENT_SCOPE)
endfunction()

function(add_static_target target ext_target libname)
  add_library(${target} STATIC IMPORTED GLOBAL)
  add_dependencies(${target} ${ext_target})
  set_target_properties(${target} PROPERTIES
    IMPORTED_LOCATION ${DEPS_DESTDIR}/lib/${libname}
  )
endfunction()



if(WITH_LTO)
  set(flto "-flto")
else()
  set(flto "")
endif()

set(cross_host "")
set(cross_rc "")
if(CMAKE_CROSSCOMPILING)
  set(cross_host "--host=${ARCH_TRIPLET}")
  if (ARCH_TRIPLET MATCHES mingw AND CMAKE_RC_COMPILER)
    set(cross_rc "WINDRES=${CMAKE_RC_COMPILER}")
  endif()
endif()


# Builds a target; takes the target name (e.g. "readline") and builds it in an external project with
# target name suffixed with `_external`.  Its upper-case value is used to get the download details
# (from the variables set above).  The following options are supported and passed through to
# ExternalProject_Add if specified.  If omitted, these defaults are used:
set(build_def_DEPENDS "")
set(build_def_PATCH_COMMAND "")
set(build_def_CONFIGURE_COMMAND ./configure ${cross_host} --disable-shared --prefix=${DEPS_DESTDIR} --with-pic
    "CC=${deps_cc}" "CXX=${deps_cxx}" "CFLAGS=-O2 ${flto}" "CXXFLAGS=-O2 ${flto}" ${cross_rc})
set(build_def_BUILD_COMMAND make)
set(build_def_INSTALL_COMMAND make install)
set(build_def_BUILD_BYPRODUCTS ${DEPS_DESTDIR}/lib/lib___TARGET___.a ${DEPS_DESTDIR}/include/___TARGET___.h)

function(build_external target)
  set(options DEPENDS PATCH_COMMAND CONFIGURE_COMMAND BUILD_COMMAND INSTALL_COMMAND BUILD_BYPRODUCTS)
  cmake_parse_arguments(PARSE_ARGV 1 arg "" "" "${options}")
  foreach(o ${options})
    if(NOT DEFINED arg_${o})
      set(arg_${o} ${build_def_${o}})
    endif()
  endforeach()
  string(REPLACE ___TARGET___ ${target} arg_BUILD_BYPRODUCTS "${arg_BUILD_BYPRODUCTS}")

  string(TOUPPER "${target}" prefix)
  expand_urls(urls ${${prefix}_SOURCE} ${${prefix}_MIRROR})
  ExternalProject_Add("${target}_external"
    DEPENDS ${arg_DEPENDS}
    BUILD_IN_SOURCE ON
    PREFIX ${DEPS_SOURCEDIR}
    URL ${urls}
    URL_HASH ${${prefix}_HASH}
    DOWNLOAD_NO_PROGRESS ON
    PATCH_COMMAND ${arg_PATCH_COMMAND}
    CONFIGURE_COMMAND ${arg_CONFIGURE_COMMAND}
    BUILD_COMMAND ${arg_BUILD_COMMAND}
    INSTALL_COMMAND ${arg_INSTALL_COMMAND}
    BUILD_BYPRODUCTS ${arg_BUILD_BYPRODUCTS}
  )
endfunction()



set(openssl_system_env "")
if(CMAKE_CROSSCOMPILING)
  if(ARCH_TRIPLET STREQUAL x86_64-w64-mingw32)
    set(openssl_system_env SYSTEM=MINGW64 RC=${CMAKE_RC_COMPILER} AR=${ARCH_TRIPLET}-ar RANLIB=${ARCH_TRIPLET}-ranlib)
  elseif(ARCH_TRIPLET STREQUAL i686-w64-mingw32)
    set(openssl_system_env SYSTEM=MINGW32 RC=${CMAKE_RC_COMPILER} AR=${ARCH_TRIPLET}-ar RANLIB=${ARCH_TRIPLET}-ranlib)
  endif()
endif()
build_external(openssl
  CONFIGURE_COMMAND ${CMAKE_COMMAND} -E env CC=${deps_cc} ${openssl_system_env} ./config
    --prefix=${DEPS_DESTDIR} no-shared no-capieng no-dso no-dtls1 no-ec_nistp_64_gcc_128 no-gost
    no-heartbeats no-md2 no-rc5 no-rdrand no-rfc3779 no-sctp no-ssl-trace no-ssl2 no-ssl3
    no-static-engine no-tests no-weak-ssl-ciphers no-zlib no-zlib-dynamic "CFLAGS=-O2 ${flto}"
  INSTALL_COMMAND make install_sw
  BUILD_BYPRODUCTS
    ${DEPS_DESTDIR}/lib/libssl.a ${DEPS_DESTDIR}/lib/libcrypto.a
    ${DEPS_DESTDIR}/include/openssl/ssl.h ${DEPS_DESTDIR}/include/openssl/crypto.h
)
add_static_target(OpenSSL::SSL openssl_external libssl.a)
add_static_target(OpenSSL::Crypto openssl_external libcrypto.a)
if(WIN32)
  target_link_libraries(OpenSSL::Crypto INTERFACE "ws2_32;crypt32;iphlpapi")
endif()

set(OPENSSL_INCLUDE_DIR ${DEPS_DESTDIR}/include)
set(OPENSSL_VERSION 1.1.1)



build_external(expat
  CONFIGURE_COMMAND ./configure ${cross_host} --prefix=${DEPS_DESTDIR} --enable-static
  --disable-shared --with-pic --without-examples --without-tests --without-docbook --without-xmlwf
  "CC=${deps_cc}" "CFLAGS=-O2 ${flto}"
)
add_static_target(expat expat_external libexpat.a)


build_external(unbound
  DEPENDS openssl_external expat_external
  CONFIGURE_COMMAND ./configure ${cross_host} ${cross_rc} --prefix=${DEPS_DESTDIR} --disable-shared
  --enable-static --with-libunbound-only --with-pic
  --$<IF:$<BOOL:${WITH_LTO}>,enable,disable>-flto --with-ssl=${DEPS_DESTDIR}
  --with-libexpat=${DEPS_DESTDIR}
  "CC=${deps_cc}" "CFLAGS=-O2 ${flto}"
)
add_static_target(libunbound unbound_external libunbound.a)
if(NOT WIN32)
  set_target_properties(libunbound PROPERTIES INTERFACE_LINK_LIBRARIES "OpenSSL::SSL;OpenSSL::Crypto")
else()
  set_target_properties(libunbound PROPERTIES INTERFACE_LINK_LIBRARIES "OpenSSL::SSL;OpenSSL::Crypto;ws2_32;crypt32;iphlpapi")
endif()



build_external(sodium)
add_static_target(sodium sodium_external libsodium.a)

build_external(sqlite3)
add_static_target(sqlite3 sqlite_external libsqlite3.a)


if(ZMQ_VERSION VERSION_LESS 4.3.3 AND CMAKE_CROSSCOMPILING AND ARCH_TRIPLET MATCHES mingw)
  set(zmq_patch PATCH_COMMAND patch -p1 -i ${PROJECT_SOURCE_DIR}/contrib/cross/patches/libzmq-pr3601-mingw-build-fix.patch
    COMMAND patch -p1 -i ${PROJECT_SOURCE_DIR}/contrib/cross/patches/libzmq-pr3613-fix-funcptr-call.patch)
endif()
build_external(zmq
  DEPENDS sodium_external
  ${zmq_patch}
  CONFIGURE_COMMAND ./configure ${cross_host} --prefix=${DEPS_DESTDIR} --enable-static --disable-shared
    --disable-curve-keygen --enable-curve --disable-drafts --disable-libunwind --with-libsodium
    --without-pgm --without-norm --without-vmci --without-docs --with-pic --disable-Werror
    "CC=${deps_cc}" "CXX=${deps_cxx}" "CFLAGS=-O2 -fstack-protector ${flto}" "CXXFLAGS=-O2 -fstack-protector ${flto}"
    "sodium_CFLAGS=-I${DEPS_DESTDIR}/include" "sodium_LIBS=-L${DEPS_DESTDIR}/lib -lsodium"
)
add_static_target(libzmq zmq_external libzmq.a)

set(libzmq_link_libs "sodium")
if(CMAKE_CROSSCOMPILING AND ARCH_TRIPLET MATCHES mingw)
  list(APPEND libzmq_link_libs iphlpapi)
endif()

set_target_properties(libzmq PROPERTIES
  INTERFACE_LINK_LIBRARIES "${libzmq_link_libs}"
  INTERFACE_COMPILE_DEFINITIONS "ZMQ_STATIC")
