
add_executable(lokinet-vpn lokinet-vpn.cpp)
if(APPLE)
  add_executable(lokinet lokinet.swift)
  enable_lto(lokinet)
else()
  add_executable(lokinet lokinet.cpp)
  enable_lto(lokinet lokinet-vpn)
endif()
if(WITH_BOOTSTRAP)
  add_executable(lokinet-bootstrap lokinet-bootstrap.cpp)
  enable_lto(lokinet-bootstrap)
endif()


if(TRACY_ROOT)
  target_sources(lokinet PRIVATE ${TRACY_ROOT}/TracyClient.cpp)
endif()

set(should_install ON)
set(SETCAP)

if(CMAKE_SYSTEM_NAME MATCHES "Linux")
  option(WITH_SETCAP "use setcap when installing" ON)
  if(WITH_SETCAP)
    find_program(SETCAP NAMES setcap HINTS /sbin /usr/sbin)
    if(SETCAP)
      message(STATUS "Found setcap binary: ${SETCAP}")
    else()
      message(WARNING "cannot find setcap binary you will not be able use the install targets unless you use -DWITH_SETCAP=OFF")
      set(should_install OFF)
    endif()
  endif()
endif()

if(WITH_BOOTSTRAP)
  target_link_libraries(lokinet-bootstrap PUBLIC cpr::cpr)
  if(NOT WIN32)
    find_package(OpenSSL REQUIRED)
    # because debian sid's curl doesn't link against openssl for some godawful cursed reason
    target_link_libraries(lokinet-bootstrap PUBLIC OpenSSL::SSL OpenSSL::Crypto)
  endif()
endif()

set(exetargets lokinet lokinet-vpn)
if(WITH_BOOTSTRAP)
  list(APPEND exetargets lokinet-bootstrap)
endif()

foreach(exe ${exetargets})
  if(WIN32 AND NOT MSVC_VERSION)
    target_sources(${exe} PRIVATE ${CMAKE_BINARY_DIR}/${exe}.rc)
    target_link_libraries(${exe} PRIVATE -static-libstdc++ -static-libgcc --static -Wl,--pic-executable,-e,mainCRTStartup,--subsystem,console:5.00)
    target_link_libraries(${exe} PRIVATE ws2_32 iphlpapi)
  elseif(CMAKE_SYSTEM_NAME MATCHES "FreeBSD")
    target_link_directories(${exe} PRIVATE /usr/local/lib)
  endif()
  target_link_libraries(${exe} PUBLIC liblokinet)
  target_include_directories(${exe} PUBLIC "${PROJECT_SOURCE_DIR}")
  target_compile_definitions(${exe} PRIVATE -DVERSIONTAG=${GIT_VERSION_REAL})
  if(should_install)
    if(APPLE)
      install(TARGETS ${exe} BUNDLE DESTINATION "${PROJECT_BINARY_DIR}" COMPONENT lokinet)
    else()
      install(TARGETS ${exe} RUNTIME DESTINATION bin COMPONENT lokinet)
    endif()
  endif()
endforeach()

if(APPLE)

  set(CODESIGN_APP "" CACHE STRING "codesign the macos app using this key identity")
  set(CODESIGN_EXT "${CODESIGN_APP}" CACHE STRING "codesign the internal extension using this key identity; defaults to CODESIGN_APP if empty")
  option(CODESIGN "codesign the resulting app and extension" ON)

  set(mac_icon ${CMAKE_CURRENT_BINARY_DIR}/lokinet.icns)
  add_custom_command(OUTPUT ${mac_icon}
    COMMAND ${PROJECT_SOURCE_DIR}/contrib/macos/mk-icns.sh ${PROJECT_SOURCE_DIR}/contrib/lokinet.svg ${mac_icon}
    DEPENDS ${PROJECT_SOURCE_DIR}/contrib/lokinet.svg ${PROJECT_SOURCE_DIR}/contrib/macos/mk-icns.sh)
  add_custom_target(icons DEPENDS ${mac_icon})
  add_dependencies(lokinet icons lokinet-extension)
  add_custom_command(TARGET lokinet
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${PROJECT_SOURCE_DIR}/contrib/bootstrap/mainnet.signed
        $<TARGET_BUNDLE_DIR:lokinet-extension>/Contents/Resources/bootstrap.signed
    COMMAND mkdir -p $<TARGET_BUNDLE_DIR:lokinet>/Contents/PlugIns
    COMMAND cp -a $<TARGET_BUNDLE_DIR:lokinet-extension> $<TARGET_BUNDLE_DIR:lokinet>/Contents/PlugIns/
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${PROJECT_SOURCE_DIR}/contrib/macos/lokinet.provisionprofile
        $<TARGET_BUNDLE_DIR:lokinet>/Contents/embedded.provisionprofile
    )

  set_target_properties(lokinet
    PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_INFO_STRING "Lokinet IP Packet Onion Router"
    MACOSX_BUNDLE_BUNDLE_NAME "Lokinet"
    MACOSX_BUNDLE_BUNDLE_VERSION "${lokinet_VERSION}"
    MACOSX_BUNDLE_LONG_VERSION_STRING "${lokinet_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${lokinet_VERSION_MAJOR}.${lokinet_VERSION_MINOR}"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.loki-project.lokinet"
    MACOSX_BUNDLE_INFO_PLIST "${PROJECT_SOURCE_DIR}/contrib/macos/Info.plist.in"
    MACOSX_BUNDLE_ICON_FILE "${mac_icon}"
    MACOSX_BUNDLE_COPYRIGHT "© 2021, The Oxen Project")
  if(NOT CODESIGN)
    message(STATUS "codesigning disabled")
    add_custom_target(
      sign
      DEPENDS lokinet lokinet-extension
      COMMAND "true")
  elseif (CODESIGN_APP AND CODESIGN_EXT)
    message(STATUS "codesigning with ${CODESIGN_APP} (app) ${CODESIGN_EXT} (appex)")
    set(SIGN_TARGET "${CMAKE_CURRENT_BINARY_DIR}/lokinet.app")
    configure_file(
      "${PROJECT_SOURCE_DIR}/contrib/macos/sign.sh.in"
      "${PROJECT_BINARY_DIR}/sign.sh"
      @ONLY)
    add_custom_target(
      sign
      DEPENDS "${PROJECT_BINARY_DIR}/sign.sh" lokinet lokinet-extension
      COMMAND "${PROJECT_BINARY_DIR}/sign.sh"
      )
  else()
    message(FATAL_ERROR "CODESIGN_APP (=${CODESIGN_APP}) and/or CODESIGN_EXT (=${CODESIGN_EXT}) are not set. To disable code signing use -DCODESIGN=OFF")
  endif()
endif()

if(SETCAP)
  install(CODE "execute_process(COMMAND ${SETCAP} cap_net_admin,cap_net_bind_service=+eip ${CMAKE_INSTALL_PREFIX}/bin/lokinet)")
endif()
