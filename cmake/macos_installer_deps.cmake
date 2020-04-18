# macos specific cpack stuff goes here

# Here we build lokinet-network-control-panel into 'lokinet-gui.app' in "extra/" where a postinstall
# script will then move it to /Applications/.

set(LOKINET_GUI_REPO "https://github.com/loki-project/loki-network-control-panel.git"
    CACHE STRING "Can be set to override the default lokinet-gui git repository")
set(LOKINET_GUI_CHECKOUT ""
    CACHE STRING "Can be set to specify a particular branch or tag to build from LOKINET_GUI_REPO")

include(ExternalProject)

message(STATUS "Building LokinetGUI.app from ${LOKINET_GUI_REPO} @ ${LOKINET_GUI_BRANCH}")

ExternalProject_Add(lokinet-gui
    GIT_REPOSITORY "${LOKINET_GUI_REPO}"
    GIT_TAG "${LOKINET_GUI_CHECKOUT}"
    CMAKE_ARGS -DMACOS_APP=ON -DCMAKE_INSTALL_PREFIX=${PROJECT_BINARY_DIR}
    )


install(DIRECTORY ${PROJECT_BINARY_DIR}/LokinetGUI.app
        DESTINATION "../../Applications"
        COMPONENT gui
        PATTERN "*"
        )

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

# The GUI is GPLv3, and so the bundled core+GUI must be as well:
set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/contrib/gpl-3.0.txt")
