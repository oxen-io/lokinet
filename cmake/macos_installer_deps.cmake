# macos specific cpack stuff goes here

return()

# Here we build lokinet-network-control-panel into 'lokinet-gui.app' in "extra/" where a postinstall
# script will then move it to /Applications/.
set(LOKINET_GUI_REPO "https://github.com/oxen-io/loki-network-control-panel.git"
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

if(NOT BUILD_STATIC_DEPS)
    message(FATAL_ERROR "Building an installer on macos requires -DBUILD_STATIC_DEPS=ON")
endif()

#set(CPACK_GENERATOR "Bundle")

#set(MACOSX_BUNDLE_BUNDLE_NAME Lokinet)
#set(CPACK_BUNDLE_NAME Lokinet)
#set(CPACK_BUNDLE_PLIST ${CMAKE_SOURCE_DIR}/contrib/macos/Info.plist)
#set(CPACK_BUNDLE_ICON "${CMAKE_CURRENT_BINARY_DIR}/lokinet.icns")
#set(CPACK_BUNDLE_STARTUP_COMMAND ${CMAKE_BINARY_DIR}/daemon/lokinet)
#set(MACOSX_BUNDLE_GUI_IDENTIFIER org.lokinet.lokinet)
#set(MACOSX_BUNDLE_INFO_STRING "Lokinet IP Packet Onion Router")
#set(MACOSX_BUNDLE_LONG_VERSION_STRING ${PROJECT_VERSION})
#set(MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION})
#set(MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION})
#set(MACOSX_BUNDLE_COPYRIGHT "© 2021, The Loki Project")
#set(CPACK_BUNDLE_APPLE_ENTITLEMENTS ${CMAKE_SOURCE_DIR}/contrib/macos/lokinet.entitlements)
#set(CPACK_BUNDLE_APPLE_CERT_APP "${MACOS_SIGN_APP}")

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
