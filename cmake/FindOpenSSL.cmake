if(BUILD_STATIC_DEPS)
    set(OPENSSL_ROOT_DIR ${CMAKE_BINARY_DIR}/static-deps)
    set(OPENSSL_CRYPTO_LIBRARY ${OPENSSL_ROOT_DIR}/lib/libcrypto.a)
    set(OPENSSL_INCLUDE_DIR ${OPENSSL_ROOT_DIR}/include)
else()
  find_package(PkgConfig)
  if (PKG_CONFIG_FOUND)
    pkg_check_modules(PC_OPENSSL QUIET openssl)
    set(OPENSSL_CRYPTO_LIBRARY ${PC_OPENSSL_LIBRARIES})
    set(OPENSSL_INCLUDE_DIR ${PC_OPENSSL_INCLUDE_DIRS})
  endif()
endif()


include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(OpenSSL DEFAULT_MSG
                                  OPENSSL_CRYPTO_LIBRARY
                                  OPENSSL_INCLUDE_DIR)

mark_as_advanced(OPENSSL_INCLUDE_DIR OPENSSL_CRYPTO_LIBRARY)
