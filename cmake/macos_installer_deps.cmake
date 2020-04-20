# macos specific cpack stuff goes here

# Here we build lokinet-network-control-panel into 'lokinet-gui.app' in "extra/" where a postinstall
# script will then move it to /Applications/.

set(LOKINET_GUI_REPO "https://github.com/loki-project/loki-network-control-panel.git"
    CACHE STRING "Can be set to override the default lokinet-gui git repository")
set(LOKINET_GUI_CHECKOUT "origin/master"
    CACHE STRING "Can be set to specify a particular branch or tag to build from LOKINET_GUI_REPO")
set(MACOS_SIGN_APP ""  # FIXME: it doesn't use a Apple Distribution key because WTF knows.
    CACHE STRING "enable codesigning of the stuff inside the .app and the lokinet binary -- use a 'Apple Distribution' key (or description) from `security find-identity -v`")
set(MACOS_SIGN_PKG ""
    CACHE STRING "enable codesigning of the .pkg -- use a 'Developer ID Installer' key (or description) from `security find-identity -v`")
set(MACOS_NOTARIZE_USER ""
    CACHE STRING "set macos notarization username; can also set it in ~/.notarization.cmake")
set(MACOS_NOTARIZE_PASS ""
    CACHE STRING "set macos notarization password; can also set it in ~/.notarization.cmake")
set(MACOS_NOTARIZE_ASC ""
    CACHE STRING "set macos notarization asc provider; can also set it in ~/.notarization.cmake")

include(ExternalProject)

message(STATUS "Building LokinetGUI.app from ${LOKINET_GUI_REPO} @ ${LOKINET_GUI_CHECKOUT}")

ExternalProject_Add(lokinet-gui
    GIT_REPOSITORY "${LOKINET_GUI_REPO}"
    GIT_TAG "${LOKINET_GUI_CHECKOUT}"
    CMAKE_ARGS -DMACOS_APP=ON -DCMAKE_INSTALL_PREFIX=${PROJECT_BINARY_DIR} -DMACOS_SIGN=${MACOS_SIGN_APP}
    )




install(DIRECTORY ${PROJECT_BINARY_DIR}/LokinetGUI.app
        DESTINATION "../../Applications"
        USE_SOURCE_PERMISSIONS
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
set(CPACK_PACKAGING_INSTALL_PREFIX "/opt/lokinet")
set(CPACK_POSTFLIGHT_LOKINET_SCRIPT ${CMAKE_SOURCE_DIR}/contrib/macos/postinstall)

# The GUI is GPLv3, and so the bundled core+GUI must be as well:
set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/contrib/gpl-3.0.txt")

set(CPACK_PRODUCTBUILD_IDENTITY_NAME "${MACOS_SIGN_PKG}")

if(MACOS_SIGN_APP)
    add_custom_target(sign ALL
        echo "Signing lokinet and lokinetctl binaries"
        COMMAND codesign -s "${MACOS_SIGN_APP}" --strict --options runtime --force -vvv $<TARGET_FILE:lokinet> $<TARGET_FILE:lokinetctl>
        DEPENDS lokinet lokinetctl
        )
endif()

if(MACOS_SIGN_APP AND MACOS_SIGN_PKG)
    if(NOT MACOS_NOTORIZE_USER)
        if(EXISTS "$ENV{HOME}/.notarization.cmake")
            include("$ENV{HOME}/.notarization.cmake")
        endif()
    endif()
    if(MACOS_NOTORIZE_USER AND MACOS_NOTORIZE_PASS AND MACOS_NOTORIZE_ASC)
        message(STATUS "'notarization' target enabled")
        configure_file(${CMAKE_SOURCE_DIR}/contrib/macos/notarize.py.in ${CMAKE_CURRENT_BINARY_DIR}/contrib/notarize.py ESCAPE_QUOTES @ONLY)
        file(COPY ${CMAKE_CURRENT_BINARY_DIR}/contrib/notarize.py DESTINATION ${PROJECT_BINARY_DIR} FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)
        add_custom_target(notarize ./notarize.py)
    else()
        message(WARNING "Not enable 'notarization' target: signing is enabled but notarization info not provided. Create ~/.notarization.cmake or set cmake parameters directly")
    endif()
endif()
