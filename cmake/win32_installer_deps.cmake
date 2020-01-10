set(TUNTAP_URL "https://build.openvpn.net/downloads/releases/latest/tap-windows-latest-stable.exe")
set(TUNTAP_EXE "${CMAKE_BINARY_DIR}/tuntap-install.exe")
file(DOWNLOAD
    ${TUNTAP_URL}
    ${TUNTAP_EXE})
install(PROGRAMS ${TUNTAP_EXE} DESTINATION bin)
set(CPACK_NSIS_DEFINES "RequestExecutionLevel admin")
set(CPACK_NSIS_EXTRA_INSTALL_COMMANDS "ExecWait '$INSTDIR\\\\bin\\\\tuntap-install.exe /S'")
set(CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS "ExecWait 'C:\\\\Program Files\\\\TAP-Windows\\\\Uninstall.exe /S'")
