# macos specific cpack stuff goes here
set(CPACK_GENERATOR "productbuild")
set(CPACK_PACKAGING_INSTALL_PREFIX "/usr/local")
set(CPACK_POSTFLIGHT_LOKINET_SCRIPT ${CMAKE_SOURCE_DIR}/contrib/macos/postinstall)
