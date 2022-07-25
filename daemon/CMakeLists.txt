set(exetargets lokinet)

if(APPLE)
  add_executable(lokinet lokinet.swift)
else()
  add_executable(lokinet lokinet.cpp)
endif()
add_executable(lokinet-vpn lokinet-vpn.cpp)
enable_lto(lokinet lokinet-vpn)
list(APPEND exetargets lokinet-vpn)

if(WITH_BOOTSTRAP)
  add_executable(lokinet-bootstrap lokinet-bootstrap.cpp)
  list(APPEND exetargets lokinet-bootstrap)
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
  if(should_install)
    if(APPLE)
      install(TARGETS ${exe}
        BUNDLE DESTINATION "${PROJECT_BINARY_DIR}"
        RUNTIME DESTINATION "."
        COMPONENT lokinet)
    else()
      install(TARGETS ${exe} RUNTIME DESTINATION bin COMPONENT lokinet)
    endif()
  endif()
endforeach()

if(APPLE)
  option(MACOS_SYSTEM_EXTENSION
    "Build the network extension as a system extension rather than a plugin.  This must be ON for non-app store release builds, and must be OFF for dev builds and Mac App Store distribution builds"
    OFF)
  option(CODESIGN "codesign the resulting app and extension" ON)
  set(CODESIGN_ID "" CACHE STRING "codesign the macos app using this key identity; if empty we'll try to guess")
  set(default_profile_type "dev")
  if(MACOS_SYSTEM_EXTENSION)
    set(default_profile_type "release")
  endif()
  set(CODESIGN_PROFILE "${PROJECT_SOURCE_DIR}/contrib/macos/lokinet.${default_profile_type}.provisionprofile" CACHE FILEPATH
    "Path to a .provisionprofile to use for the main app")
  set(CODESIGN_EXT_PROFILE "${PROJECT_SOURCE_DIR}/contrib/macos/lokinet-extension.${default_profile_type}.provisionprofile" CACHE FILEPATH
    "Path to a .provisionprofile to use for the extension")

  if(CODESIGN AND NOT CODESIGN_ID)
    if(MACOS_SYSTEM_EXTENSION)
      set(codesign_cert_pattern "Developer ID Application")
    else()
      set(codesign_cert_pattern "Apple Development")
    endif()
    execute_process(
      COMMAND security find-identity -v -p codesigning
      COMMAND sed -n "s/^ *[0-9][0-9]*)  *\\([A-F0-9]\\{40\\}\\)  *\"\\(${codesign_cert_pattern}.*\\)\"\$/\\1 \\2/p"
      RESULT_VARIABLE find_id_exit_code
      OUTPUT_VARIABLE find_id_output)
    if(NOT find_id_exit_code EQUAL 0)
      message(FATAL_ERROR "Finding signing identities with security find-identity failed; try specifying an id using -DCODESIGN_ID=...")
    endif()

    string(REGEX MATCHALL "(^|\n)[0-9A-F]+" find_id_sign_id "${find_id_output}")
    if(NOT find_id_sign_id)
      message(FATAL_ERROR "Did not find any \"${codesign_cert_pattern}\" identity; try specifying an id using -DCODESIGN_ID=...")
    endif()
    if (find_id_sign_id MATCHES ";")
      message(FATAL_ERROR "Found multiple \"${codesign_cert_pattern}\" identities:\n${find_id_output}\nSpecify an identify using -DCODESIGN_ID=...")
    endif()
    set(CODESIGN_ID "${find_id_sign_id}" CACHE STRING "" FORCE)
  endif()

  if(CODESIGN)
    message(STATUS "Codesigning using ${CODESIGN_ID}")
  else()
    message(WARNING "Codesigning disabled; the resulting build will not run on most macOS systems")
  endif()

  if(MACOS_SYSTEM_EXTENSION)
    set(lokinet_ext_dir Contents/Library/SystemExtensions)
    target_compile_definitions(lokinet PRIVATE MACOS_SYSTEM_EXTENSION)
    if (NOT MACOS_NOTARIZE_USER AND NOT MACOS_NOTARIZE_PASS AND NOT MACOS_NOTARIZE_ASC AND EXISTS "$ENV{HOME}/.notarization.cmake")
      message(STATUS "Loading notarization info from ~/.notarization.cmake")
      include("$ENV{HOME}/.notarization.cmake")
    endif()
    if (MACOS_NOTARIZE_USER AND MACOS_NOTARIZE_PASS AND MACOS_NOTARIZE_ASC)
      message(STATUS "Enabling notarization with account ${MACOS_NOTARIZE_ASC}/${MACOS_NOTARIZE_USER}")
    else()
      message(WARNING "You have not set one or more of MACOS_NOTARIZE_USER, MACOS_NOTARIZE_PASS, MACOS_NOTARIZE_ASC: notarization will fail; see contrib/macos/README.txt")
    endif()
  else()
    set(lokinet_ext_dir Contents/PlugIns)
  endif()

  foreach(var CODESIGN_PROFILE CODESIGN_EXT_PROFILE)
    if(NOT ${var})
      message(WARNING "Missing a ${var} provisioning profile, and not building a system extension: Apple will most likely log an uninformative error message to the system log and then kill harmless kittens if you try to run the result")
    endif()
    if(NOT EXISTS "${${var}}")
      message(FATAL_ERROR "Provisioning profile ${${var}} does not exist; fix your -D${var} path")
    endif()
  endforeach()
  message(STATUS "Using ${CODESIGN_PROFILE} provisioning profile")
  message(STATUS "Using ${CODESIGN_EXT_PROFILE} extension provisioning profile")

  set(mac_icon ${CMAKE_CURRENT_BINARY_DIR}/lokinet.icns)
  add_custom_command(OUTPUT ${mac_icon}
    COMMAND ${PROJECT_SOURCE_DIR}/contrib/macos/mk-icns.sh ${PROJECT_SOURCE_DIR}/contrib/lokinet.svg ${mac_icon}
    DEPENDS ${PROJECT_SOURCE_DIR}/contrib/lokinet.svg ${PROJECT_SOURCE_DIR}/contrib/macos/mk-icns.sh)
  add_custom_target(icons DEPENDS ${mac_icon})
  add_dependencies(lokinet icons lokinet-extension)
  set(post_build_pp)
  if(CODESIGN AND CODESIGN_PROFILE)
    set(post_build_pp COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CODESIGN_PROFILE}
        $<TARGET_BUNDLE_DIR:lokinet>/Contents/embedded.provisionprofile)
  endif()

  set_target_properties(lokinet
    PROPERTIES
    OUTPUT_NAME Lokinet
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_INFO_STRING "Lokinet IP Packet Onion Router"
    MACOSX_BUNDLE_BUNDLE_NAME "Lokinet"
    MACOSX_BUNDLE_BUNDLE_VERSION "${lokinet_VERSION}"
    MACOSX_BUNDLE_LONG_VERSION_STRING "${lokinet_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${lokinet_VERSION_MAJOR}.${lokinet_VERSION_MINOR}"
    MACOSX_BUNDLE_GUI_IDENTIFIER "org.lokinet"
    MACOSX_BUNDLE_INFO_PLIST "${PROJECT_SOURCE_DIR}/contrib/macos/lokinet.Info.plist.in"
    MACOSX_BUNDLE_ICON_FILE "${mac_icon}"
    MACOSX_BUNDLE_COPYRIGHT "© 2022, The Oxen Project"
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}"
  )

  add_custom_target(assemble
    DEPENDS lokinet lokinet-extension
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${PROJECT_SOURCE_DIR}/contrib/bootstrap/mainnet.signed
      $<TARGET_BUNDLE_DIR:lokinet-extension>/Contents/Resources/bootstrap.signed
    COMMAND mkdir -p $<TARGET_BUNDLE_DIR:lokinet>/${lokinet_ext_dir}
    COMMAND cp -a $<TARGET_BUNDLE_DIR:lokinet-extension> $<TARGET_BUNDLE_DIR:lokinet>/${lokinet_ext_dir}
    ${post_build_pp})

  if(TARGET lokinet-gui)
    add_custom_target(copy_gui
      DEPENDS lokinet lokinet-extension lokinet-gui
      # FIXME: we really shouldn't be building inside the source directory but this is npm...
      COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_SOURCE_DIR}/lokinet-gui/release/mac/lokinet-gui.app $<TARGET_BUNDLE_DIR:lokinet>
    )
    add_dependencies(assemble copy_gui)
  endif()

  if(NOT CODESIGN)
    message(STATUS "codesigning disabled")
    add_custom_target(
      sign
      DEPENDS assemble
      COMMAND "true")

  elseif(CODESIGN)
    set(SIGN_TARGET "${PROJECT_BINARY_DIR}/Lokinet.app")
    if(MACOS_SYSTEM_EXTENSION)
      set(LOKINET_ENTITLEMENTS_TYPE sysext)
    else()
      set(LOKINET_ENTITLEMENTS_TYPE plugin)
    endif()
    configure_file(
      "${PROJECT_SOURCE_DIR}/contrib/macos/sign.sh.in"
      "${PROJECT_BINARY_DIR}/sign.sh"
      @ONLY)
    add_custom_target(
      sign
      DEPENDS "${PROJECT_BINARY_DIR}/sign.sh" assemble
      COMMAND "${PROJECT_BINARY_DIR}/sign.sh"
      )

    if(NOT (MACOS_NOTARIZE_USER AND MACOS_NOTARIZE_PASS AND MACOS_NOTARIZE_ASC))
      message(WARNING "You have not set one or more of MACOS_NOTARIZE_USER, MACOS_NOTARIZE_PASS, MACOS_NOTARIZE_ASC: notarization disabled")
    endif()
    if (MACOS_SYSTEM_EXTENSION)
      set(notarize_py_is_sysext True)
    else()
      set(notarize_py_is_sysext False)
    endif()
    configure_file(
      "${PROJECT_SOURCE_DIR}/contrib/macos/notarize.py.in"
      "${PROJECT_BINARY_DIR}/notarize.py"
      @ONLY)
    add_custom_target(
      notarize
      DEPENDS "${PROJECT_BINARY_DIR}/notarize.py" sign
      COMMAND "${PROJECT_BINARY_DIR}/notarize.py"
      )

  else()
    message(FATAL_ERROR "CODESIGN_APP (=${CODESIGN_APP}) and/or CODESIGN_EXT (=${CODESIGN_EXT}) are not set. To disable code signing use -DCODESIGN=OFF")
  endif()
endif()

if(SETCAP)
  install(CODE "execute_process(COMMAND ${SETCAP} cap_net_admin,cap_net_bind_service=+eip ${CMAKE_INSTALL_PREFIX}/bin/lokinet)")
endif()
