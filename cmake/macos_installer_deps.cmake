# macos specific cpack stuff goes here

# Here we copy 'lokinet-gui.app' into "extra/" where a postinstall script will then move it to
# /Applications/. The app bundle (lokinet-gui.app) should be built from the lokinet gui repository
# and then copied into the lokinet source tree where this install command will find it.
# 
# TODO: 1) avoid "extra/" here -- this is a hack that works with postinstall script to place this
#          in /Applications. this means it does nothing useful with "make install"
# TODO: 2) review permissions here. something odd is happening between moving and copying this
#          app bundle around. but we shouldn't need such loose permissions.
# TODO: 3) avoid the need to manually copy 'lokinet-gui.app' into place
install(DIRECTORY ${CMAKE_SOURCE_DIR}/lokinet-gui.app
        DESTINATION "extra"
        COMPONENT gui
        PATTERN "*"
        PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ
                    GROUP_EXECUTE GROUP_WRITE GROUP_READ
                    WORLD_EXECUTE WORLD_WRITE WORLD_READ)

# copy files that will be later moved by the postinstall script to proper locations
install(FILES ${CMAKE_SOURCE_DIR}/contrib/macos/lokinet_macos_daemon_script.sh
              ${CMAKE_SOURCE_DIR}/contrib/macos/network.loki.lokinet.daemon.plist
        DESTINATION "extra/"
        COMPONENT lokinet)

set(CPACK_COMPONENTS_ALL lokinet gui)

set(CPACK_COMPONENT_LOKINET_DISPLAY_NAME "Lokinet Service")
set(CPACK_COMPONENT_LOKINET_DESCRIPTION "Main Lokinet runtime service, managed by Launchd")

set(CPACK_COMPONENT_GUI_DISPLAY_NAME "Lokinet GUI")
set(CPACK_COMPONENT_GUI_DESCRIPTION "Small GUI which provides stats and limited runtime control of the Lokinet service. Resides in the system tray.")

set(CPACK_GENERATOR "productbuild")
set(CPACK_PACKAGING_INSTALL_PREFIX "/usr/local")
set(CPACK_POSTFLIGHT_LOKINET_SCRIPT ${CMAKE_SOURCE_DIR}/contrib/macos/postinstall)
