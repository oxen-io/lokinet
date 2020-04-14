# macos specific cpack stuff goes here

# copy files that will be later moved by the postinstall script to proper locations
install(FILES ${CMAKE_SOURCE_DIR}/contrib/macos/lokinet_macos_daemon_script.sh
              ${CMAKE_SOURCE_DIR}/contrib/macos/network.loki.lokinet.daemon.plist
	    DESTINATION "extra/")

set(CPACK_GENERATOR "productbuild")
set(CPACK_PACKAGING_INSTALL_PREFIX "/usr/local")
set(CPACK_POSTFLIGHT_LOKINET_SCRIPT ${CMAKE_SOURCE_DIR}/contrib/macos/postinstall)
