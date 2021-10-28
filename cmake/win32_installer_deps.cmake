if(NOT GUI_ZIP_URL)
  set(GUI_ZIP_URL "https://oxen.rocks/oxen-io/loki-network-control-panel/lokinet-gui-windows-32bit-v0.3.8.zip")
  set(GUI_ZIP_HASH_OPTS EXPECTED_HASH SHA256=60c2b738cf997e5684f307e5222498fd09143d495a932924105a49bf59ded8bb)
endif()

set(TUNTAP_URL "https://build.openvpn.net/downloads/releases/latest/tap-windows-latest-stable.exe")
set(TUNTAP_EXE "${CMAKE_BINARY_DIR}/tuntap-install.exe")
set(BOOTSTRAP_FILE "${PROJECT_SOURCE_DIR}/contrib/bootstrap/mainnet.signed")

file(DOWNLOAD
    ${TUNTAP_URL}
    ${TUNTAP_EXE})

file(DOWNLOAD
    ${GUI_ZIP_URL}
    ${CMAKE_BINARY_DIR}/lokinet-gui.zip
    ${GUI_ZIP_HASH_OPTS})

execute_process(COMMAND ${CMAKE_COMMAND} -E tar xf ${CMAKE_BINARY_DIR}/lokinet-gui.zip
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

install(DIRECTORY ${CMAKE_BINARY_DIR}/gui DESTINATION share COMPONENT gui)
install(PROGRAMS ${TUNTAP_EXE} DESTINATION bin COMPONENT tuntap)
install(FILES ${BOOTSTRAP_FILE} DESTINATION share COMPONENT lokinet RENAME bootstrap.signed)

set(CPACK_PACKAGE_INSTALL_DIRECTORY "Lokinet")
set(CPACK_NSIS_MUI_ICON "${CMAKE_SOURCE_DIR}/win32-setup/lokinet.ico")
set(CPACK_NSIS_DEFINES "RequestExecutionLevel admin")
set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
set(CPACK_NSIS_EXTRA_INSTALL_COMMANDS "ifFileExists $INSTDIR\\\\bin\\\\tuntap-install.exe 0 +2\\nExecWait '$INSTDIR\\\\bin\\\\tuntap-install.exe /S'\\nExecWait '$INSTDIR\\\\bin\\\\lokinet.exe --install'\\nExecWait 'sc failure lokinet reset= 60 actions= restart/1000'\\nExecWait '$INSTDIR\\\\bin\\\\lokinet.exe -g C:\\\\ProgramData\\\\lokinet\\\\lokinet.ini'\\nCopyFiles '$INSTDIR\\\\share\\\\bootstrap.signed' C:\\\\ProgramData\\\\lokinet\\\\bootstrap.signed\\n")
set(CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS "ExecWait 'net stop lokinet'\\nExecWait 'taskkill /f /t /im lokinet-gui.exe'\\nExecWait '$INSTDIR\\\\bin\\\\lokinet.exe --remove'\\nRMDir /r /REBOOTOK C:\\\\ProgramData\\\\lokinet")
set(CPACK_NSIS_CREATE_ICONS_EXTRA
    "CreateShortCut '$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\Lokinet.lnk' '$INSTDIR\\\\share\\\\gui\\\\lokinet-gui.exe'"
)
set(CPACK_NSIS_DELETE_ICONS_EXTRA
    "Delete '$SMPROGRAMS\\\\$START_MENU\\\\Lokinet.lnk'"
)

get_cmake_property(CPACK_COMPONENTS_ALL COMPONENTS)
list(REMOVE_ITEM CPACK_COMPONENTS_ALL "Unspecified")
