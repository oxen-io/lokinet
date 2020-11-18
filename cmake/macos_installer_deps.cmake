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

add_subdirectory(${PROJECT_SOURCE_DIR}/contrib/macos/uninstaller)

message(STATUS "Building LokinetGUI.app from ${LOKINET_GUI_REPO} @ ${LOKINET_GUI_CHECKOUT}")

if(NOT BUILD_STATIC_DEPS)
    message(FATAL_ERROR "Building an installer on macos requires -DBUILD_STATIC_DEPS=ON")
endif()



ExternalProject_Add(lokinet-gui
    DEPENDS lokimq::lokimq
    GIT_REPOSITORY "${LOKINET_GUI_REPO}"
    GIT_TAG "${LOKINET_GUI_CHECKOUT}"
    CMAKE_ARGS -DMACOS_APP=ON -DCMAKE_INSTALL_PREFIX=${PROJECT_BINARY_DIR} -DMACOS_SIGN=${MACOS_SIGN_APP}
        -DCMAKE_PREFIX_PATH=${CMAKE_PREFIX_PATH} -DBUILD_SHARED_LIBS=OFF
        "-DLOKIMQ_LIBRARIES=$<TARGET_FILE:lokimq::lokimq>$<SEMICOLON>$<TARGET_FILE:libzmq>$<SEMICOLON>$<TARGET_FILE:sodium>"
        "-DLOKIMQ_INCLUDE_DIRS=$<TARGET_PROPERTY:lokimq::lokimq,INCLUDE_DIRECTORIES>"
        )

install(PROGRAMS ${CMAKE_SOURCE_DIR}/contrib/macos/lokinet_uninstall.sh
        DESTINATION "bin/"
        COMPONENT lokinet)

install(DIRECTORY ${PROJECT_BINARY_DIR}/LokinetGUI.app
        DESTINATION "../../Applications/Lokinet"
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
set(CPACK_PREINSTALL_LOKINET_SCRIPT ${CMAKE_SOURCE_DIR}/contrib/macos/preinstall)
set(CPACK_POSTFLIGHT_LOKINET_SCRIPT ${CMAKE_SOURCE_DIR}/contrib/macos/postinstall)

set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/LICENSE.txt")

set(CPACK_PRODUCTBUILD_IDENTITY_NAME "${MACOS_SIGN_PKG}")

if(MACOS_SIGN_APP)
    add_custom_target(sign ALL
        echo "Signing lokinet and lokinet-vpn binaries"
        COMMAND codesign -s "${MACOS_SIGN_APP}" --strict --options runtime --force -vvv $<TARGET_FILE:lokinet> $<TARGET_FILE:lokinet-vpn>
        DEPENDS lokinet lokinet-vpn
        )
endif()

if(MACOS_SIGN_APP AND MACOS_SIGN_PKG)
    if(NOT MACOS_NOTARIZE_USER)
        if(EXISTS "$ENV{HOME}/.notarization.cmake")
            include("$ENV{HOME}/.notarization.cmake")
        endif()
    endif()
    if(MACOS_NOTARIZE_USER AND MACOS_NOTARIZE_PASS AND MACOS_NOTARIZE_ASC)
        message(STATUS "'notarization' target enabled")
        configure_file(${CMAKE_SOURCE_DIR}/contrib/macos/notarize.py.in ${CMAKE_CURRENT_BINARY_DIR}/contrib/notarize.py ESCAPE_QUOTES @ONLY)
        file(COPY ${CMAKE_CURRENT_BINARY_DIR}/contrib/notarize.py DESTINATION ${PROJECT_BINARY_DIR} FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)
        add_custom_target(notarize ./notarize.py)
    else()
        message(WARNING "Not enable 'notarization' target: signing is enabled but notarization info not provided. Create ~/.notarization.cmake or set cmake parameters directly")
    endif()
endif()
