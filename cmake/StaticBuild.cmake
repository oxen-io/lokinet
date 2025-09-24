# cmake bits to do a full static build, downloading and building all dependencies.

# Most of these are CACHE STRINGs so that you can override them using -DWHATEVER during cmake
# invocation to override.

include_guard(GLOBAL)

include("${CMAKE_CURRENT_LIST_DIR}/../external/oxen-libquic/cmake/StaticBuild.cmake")

set(LOCAL_MIRROR "" CACHE STRING "local mirror path/URL for lib downloads")

set(EXPAT_VERSION 2.7.1 CACHE STRING "expat version")
string(REPLACE "." "_" EXPAT_TAG "R_${EXPAT_VERSION}")
set(EXPAT_MIRROR ${LOCAL_MIRROR} https://github.com/libexpat/libexpat/releases/download/${EXPAT_TAG}
    CACHE STRING "expat download mirror(s)")
set(EXPAT_SOURCE expat-${EXPAT_VERSION}.tar.xz)
set(EXPAT_HASH SHA512=4c9a6c1c1769d2c4404da083dd3013dbc73883da50e2b7353db2349a420e9b6d27cac7dbcb645991d6c7cdbf79bd88486fc1ac353084ce48e61081fb56e13d46
    CACHE STRING "expat source hash")

set(UNBOUND_VERSION 1.23.0 CACHE STRING "unbound version")
set(UNBOUND_MIRROR ${LOCAL_MIRROR} https://nlnetlabs.nl/downloads/unbound CACHE STRING "unbound download mirror(s)")
set(UNBOUND_SOURCE unbound-${UNBOUND_VERSION}.tar.gz)
set(UNBOUND_HASH SHA512=9b5ca48f4f5189f168f76396f5895f39262a4333e589f8c64bb9298a55c6266f626a4a4399370c68edd9f6318215a401146bf9e16a101c54decf623668a398af
    CACHE STRING "unbound source hash")

set(SQLITE3_VERSION 3500200 CACHE STRING "sqlite3 version")
set(SQLITE3_MIRROR ${LOCAL_MIRROR} https://www.sqlite.org/2025
    CACHE STRING "sqlite3 download mirror(s)")
set(SQLITE3_SOURCE sqlite-autoconf-${SQLITE3_VERSION}.tar.gz)
set(SQLITE3_HASH SHA3_256=e4d2b4332988f479ec032ccff00963a9bbd24a3a0f0222b4e249653fa680b4c0
  CACHE STRING "sqlite3 source hash")

set(SODIUM_VERSION 1.0.20 CACHE STRING "libsodium version")
set(SODIUM_MIRROR ${LOCAL_MIRROR}
  https://download.libsodium.org/libsodium/releases
  https://github.com/jedisct1/libsodium/releases/download/${SODIUM_VERSION}-RELEASE
  CACHE STRING "libsodium mirror(s)")
set(SODIUM_SOURCE libsodium-${SODIUM_VERSION}.tar.gz)
set(SODIUM_HASH SHA512=7ea165f3c1b1609790e30a16348b9dfdc5731302da00c07c65e125c8ab115c75419a5631876973600f8a4b560ca2c8267001770b68f2eb3eebc9ba095d312702
  CACHE STRING "libsodium source hash")

set(ZMQ_VERSION 4.3.5 CACHE STRING "libzmq version")
set(ZMQ_MIRROR ${LOCAL_MIRROR} https://github.com/zeromq/libzmq/releases/download/v${ZMQ_VERSION}
    CACHE STRING "libzmq mirror(s)")
set(ZMQ_SOURCE zeromq-${ZMQ_VERSION}.tar.gz)
set(ZMQ_HASH SHA512=a71d48aa977ad8941c1609947d8db2679fc7a951e4cd0c3a1127ae026d883c11bd4203cf315de87f95f5031aec459a731aec34e5ce5b667b8d0559b157952541
    CACHE STRING "libzmq source hash")

set(ZLIB_VERSION 1.3.1 CACHE STRING "zlib version")
set(ZLIB_MIRROR ${LOCAL_MIRROR} https://zlib.net
    CACHE STRING "zlib mirror(s)")
set(ZLIB_SOURCE zlib-${ZLIB_VERSION}.tar.xz)
set(ZLIB_HASH SHA256=38ef96b8dfe510d42707d9c781877914792541133e1870841463bfa73f883e32
  CACHE STRING "zlib source hash")

set(ZSTD_VERSION 1.5.7 CACHE STRING "zstd version")
set(ZSTD_MIRROR ${LOCAL_MIRROR} https://github.com/facebook/zstd/releases/download/v${ZSTD_VERSION}
    CACHE STRING "zstd mirror(s)")
set(ZSTD_SOURCE zstd-${ZSTD_VERSION}.tar.gz)
set(ZSTD_HASH SHA256=eb33e51f49a15e023950cd7825ca74a4a2b43db8354825ac24fc1b7ee09e6fa3
    CACHE STRING "zstd source hash")

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


add_library(lokinet_static_deps INTERFACE)

function(add_static_target target ext_target libname)
  add_library(${target} STATIC IMPORTED GLOBAL)
  add_dependencies(${target} ${ext_target})
  target_link_libraries(lokinet_static_deps INTERFACE ${target})
  set_target_properties(${target} PROPERTIES
    IMPORTED_LOCATION ${DEPS_DESTDIR}/lib/${libname}
  )
  if (ARGN)
    target_link_libraries(${target} INTERFACE ${ARGN})
  endif()
endfunction()


set(cross_host "")
set(cross_rc "")
if(CMAKE_CROSSCOMPILING)
  set(cross_host "--host=${ARCH_TRIPLET}")
  if (ARCH_TRIPLET MATCHES mingw AND CMAKE_RC_COMPILER)
    set(cross_rc "WINDRES=${CMAKE_RC_COMPILER}")
  endif()
endif()
if(ANDROID)
  set(android_toolchain_suffix linux-android)
  set(android_compiler_suffix linux-android23)
  if(CMAKE_ANDROID_ARCH_ABI MATCHES x86_64)
    set(android_machine x86_64)
    set(cross_host "--host=x86_64-linux-android")
    set(android_compiler_prefix x86_64)
    set(android_compiler_suffix linux-android23)
    set(android_toolchain_prefix x86_64)
    set(android_toolchain_suffix linux-android)
  elseif(CMAKE_ANDROID_ARCH_ABI MATCHES x86)
    set(android_machine x86)
    set(cross_host "--host=i686-linux-android")
    set(android_compiler_prefix i686)
    set(android_compiler_suffix linux-android23)
    set(android_toolchain_prefix i686)
    set(android_toolchain_suffix linux-android)
  elseif(CMAKE_ANDROID_ARCH_ABI MATCHES armeabi-v7a)
    set(android_machine arm)
    set(cross_host "--host=armv7a-linux-androideabi")
    set(android_compiler_prefix armv7a)
    set(android_compiler_suffix linux-androideabi23)
    set(android_toolchain_prefix arm)
    set(android_toolchain_suffix linux-androideabi)
  elseif(CMAKE_ANDROID_ARCH_ABI MATCHES arm64-v8a)
    set(android_machine arm64)
    set(cross_host "--host=aarch64-linux-android")
    set(android_compiler_prefix aarch64)
    set(android_compiler_suffix linux-android23)
    set(android_toolchain_prefix aarch64)
    set(android_toolchain_suffix linux-android)
  else()
    message(FATAL_ERROR "unknown android arch: ${CMAKE_ANDROID_ARCH_ABI}")
  endif()
  set(deps_cc "${CMAKE_ANDROID_NDK}/toolchains/llvm/prebuilt/linux-x86_64/bin/${android_compiler_prefix}-${android_compiler_suffix}-clang")
  set(deps_cxx "${CMAKE_ANDROID_NDK}/toolchains/llvm/prebuilt/linux-x86_64/bin/${android_compiler_prefix}-${android_compiler_suffix}-clang++")
  set(deps_ld "${CMAKE_ANDROID_NDK}/toolchains/llvm/prebuilt/linux-x86_64/bin/${android_compiler_prefix}-${android_toolchain_suffix}-ld")
  set(deps_ranlib "${CMAKE_ANDROID_NDK}/toolchains/llvm/prebuilt/linux-x86_64/bin/${android_toolchain_prefix}-${android_toolchain_suffix}-ranlib")
  set(deps_ar "${CMAKE_ANDROID_NDK}/toolchains/llvm/prebuilt/linux-x86_64/bin/${android_toolchain_prefix}-${android_toolchain_suffix}-ar")
endif()

set(deps_CFLAGS "-O2")
set(deps_CXXFLAGS "-O2")

if(WITH_LTO)
  set(deps_CFLAGS "${deps_CFLAGS} -flto")
endif()

if(APPLE)
  set(deps_CFLAGS "${deps_CFLAGS} -mmacosx-version-min=${CMAKE_OSX_DEPLOYMENT_TARGET}")
  set(deps_CXXFLAGS "${deps_CXXFLAGS} -mmacosx-version-min=${CMAKE_OSX_DEPLOYMENT_TARGET}")
endif()

if(_winver)
  set(deps_CFLAGS "${deps_CFLAGS} -D_WIN32_WINNT=${_winver}")
  set(deps_CXXFLAGS "${deps_CXXFLAGS} -D_WIN32_WINNT=${_winver}")
endif()


if("${CMAKE_GENERATOR}" STREQUAL "Unix Makefiles")
  set(_make $(MAKE))
else()
  set(_make make)
endif()


# Builds a target; takes the target name (e.g. "readline") and builds it in an external project with
# target name suffixed with `_external`.  Its upper-case value is used to get the download details
# (from the variables set above).  The following options are supported and passed through to
# ExternalProject_Add if specified.  If omitted, these defaults are used:
set(build_def_DEPENDS "")
set(build_def_PATCH_COMMAND "")
set(build_def_CONFIGURE_COMMAND ./configure ${cross_host} --disable-shared --prefix=${DEPS_DESTDIR} --with-pic
    "CC=${deps_cc}" "CXX=${deps_cxx}" "CFLAGS=${deps_CFLAGS}" "CXXFLAGS=${deps_CXXFLAGS}" ${cross_rc})
set(build_def_BUILD_COMMAND ${_make})
set(build_def_INSTALL_COMMAND ${_make} install)
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

  if(arg_CONFIGURE_COMMAND MATCHES "^DEFAULT_CMAKE")
      string(REGEX REPLACE "^DEFAULT_CMAKE(;?)" "CMAKE_ARGS;-DCMAKE_INSTALL_PREFIX=${DEPS_DESTDIR}\\1" configure "${arg_CONFIGURE_COMMAND}")
  else()
    set(configure CONFIGURE_COMMAND ${arg_CONFIGURE_COMMAND})
  endif()

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
    ${configure}
    BUILD_COMMAND ${arg_BUILD_COMMAND}
    INSTALL_COMMAND ${arg_INSTALL_COMMAND}
    BUILD_BYPRODUCTS ${arg_BUILD_BYPRODUCTS}
  )
endfunction()

if(NOT TARGET sodium)
  build_external(sodium CONFIGURE_COMMAND ./configure ${cross_host} ${cross_rc} --prefix=${DEPS_DESTDIR} --disable-shared
            --enable-static --with-pic "CC=${deps_cc}" "CFLAGS=${deps_CFLAGS}")
  add_static_target(sodium sodium_external libsodium.a)
endif()


if(LOKINET_PEERSTATS)
  build_external(sqlite3)
  add_static_target(sqlite3 sqlite3_external libsqlite3.a)
endif()


if(NOT TARGET libzstd::static)
  build_external(zstd
      CONFIGURE_COMMAND DEFAULT_CMAKE
        -DZSTD_BUILD_PROGRAMS=OFF -DZSTD_BUILD_TESTS=OFF -DZSTD_BUILD_STATIC=ON -DZSTD_BUILD_SHARED=OFF -DZSTD_BUILD_DICTBUILDER=OFF
      SOURCE_SUBDIR build/cmake
      BUILD_BYPRODUCTS
        ${DEPS_DESTDIR}/lib/libzstd.a
        ${DEPS_DESTDIR}/include/zstd.h
  )
  # Use the same libzstd::static target name as libsession-util so that we can use libsession's
  # static zstd if we are being built as part of libsession:
  add_static_target(libzstd::static zstd_external libzstd.a)
endif()


if(LOKINET_FULL)

  build_external(zlib
    CONFIGURE_COMMAND ${CMAKE_COMMAND} -E env "CC=${deps_cc}" "CFLAGS=${deps_CFLAGS} -fPIC" ${cross_extra} ./configure --prefix=${DEPS_DESTDIR} --static
    BUILD_BYPRODUCTS
      ${DEPS_DESTDIR}/lib/libz.a
      ${DEPS_DESTDIR}/include/zlib.h
  )
  add_static_target(zlib zlib_external libz.a)

  build_external(expat
    CONFIGURE_COMMAND ./configure ${cross_host} --prefix=${DEPS_DESTDIR} --enable-static
    --disable-shared --with-pic --without-examples --without-tests --without-docbook --without-xmlwf
    "CC=${deps_cc}" "CFLAGS=${deps_CFLAGS}"
  )
  add_static_target(expat expat_external libexpat.a)


  if(WIN32)
    set(unbound_patch
      PATCH_COMMAND ${PROJECT_SOURCE_DIR}/contrib/apply-patches.sh
          ${PROJECT_SOURCE_DIR}/contrib/patches/unbound-delete-crash-fix.patch)
  endif()
  build_external(unbound
    DEPENDS nettle_external expat_external
    ${unbound_patch}
    CONFIGURE_COMMAND ./configure ${cross_host} ${cross_rc} --prefix=${DEPS_DESTDIR}
    --with-libunbound-only --disable-shared --enable-static
    --with-pic --$<IF:$<BOOL:${WITH_LTO}>,enable,disable>-flto
    --with-nettle=${DEPS_DESTDIR} --with-libexpat=${DEPS_DESTDIR}
    --without-ssl
    "CC=${deps_cc}" "CFLAGS=${deps_CFLAGS}" "LDFLAGS=${unbound_ldflags}"
  )
  add_static_target(libunbound unbound_external libunbound.a)
  if(NOT WIN32)
    set_target_properties(libunbound PROPERTIES INTERFACE_LINK_LIBRARIES "hogweed::hogweed;nettle::nettle")
  else()
    set_target_properties(libunbound PROPERTIES INTERFACE_LINK_LIBRARIES "hogweed::hogweed;nettle::nettle;ws2_32;crypt32;iphlpapi")
  endif()



  if(ARCH_TRIPLET MATCHES mingw)
    option(WITH_WEPOLL "use wepoll zmq poller (crashy)" OFF)
    if(WITH_WEPOLL)
      set(zmq_extra --with-poller=wepoll)
    endif()
  endif()


  build_external(zmq
    DEPENDS sodium_external
    CONFIGURE_COMMAND ./configure ${cross_host} --prefix=${DEPS_DESTDIR} --enable-static --disable-shared
      --disable-curve-keygen --enable-curve --disable-drafts --disable-libunwind --with-libsodium
      --without-pgm --without-norm --without-vmci --without-docs --with-pic --disable-Werror --disable-libbsd ${zmq_extra}
      "CC=${deps_cc}" "CXX=${deps_cxx}" "CFLAGS=${deps_CFLAGS} -fstack-protector" "CXXFLAGS=${deps_CXXFLAGS} -fstack-protector"
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

endif(LOKINET_FULL)
