if(APPLE)
  option(WITH_SWIFT "use swift" ON)
  if(WITH_SWIFT)
    add_executable(lokinet lokinet.swift)
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/Lokinet.modulemap.in ${CMAKE_CURRENT_BINARY_DIR}/swift/LokinetExtension/module.modulemap ESCAPE_QUOTES @ONLY)
    target_include_directories(lokinet PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/swift)
  else()
    add_executable(lokinet lokinet.mm)
  endif()
  target_link_libraries(lokinet PUBLIC lokinet-extension)
else()
  add_executable(lokinet lokinet.cpp)
endif()
add_executable(lokinet-vpn lokinet-vpn.cpp)
add_executable(lokinet-bootstrap lokinet-bootstrap.cpp)
enable_lto(lokinet lokinet-vpn lokinet-bootstrap)

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

target_link_libraries(lokinet-bootstrap PUBLIC cpr::cpr)
if(NOT WIN32)
  find_package(OpenSSL REQUIRED)
  # because debian sid's curl doesn't link against openssl for some godawful cursed reason
  target_link_libraries(lokinet-bootstrap PUBLIC OpenSSL::SSL OpenSSL::Crypto)
endif()

set(exetargets lokinet-vpn lokinet-bootstrap)
if(NOT APPLE)
  set(exetargets lokinet ${exes})
endif()

foreach(exe ${exetargets})
  if(WIN32 AND NOT MSVC_VERSION)
    target_sources(${exe} PRIVATE ../llarp/win32/version.rc)
    target_link_libraries(${exe} PRIVATE -static-libstdc++ -static-libgcc --static -Wl,--pic-executable,-e,mainCRTStartup,--subsystem,console:5.00)
    target_link_libraries(${exe} PRIVATE ws2_32 iphlpapi)
  elseif(CMAKE_SYSTEM_NAME MATCHES "FreeBSD")
    target_link_directories(${exe} PRIVATE /usr/local/lib)
  endif()
  target_link_libraries(${exe} PRIVATE liblokinet)
  if(WITH_JEMALLOC)
    target_link_libraries(${exe} PUBLIC jemalloc)
  endif()
  target_include_directories(${exe} PRIVATE ${CMAKE_SOURCE_DIR})
  target_compile_definitions(${exe} PRIVATE -DVERSIONTAG=${GIT_VERSION_REAL})
  add_log_tag(${exe})
  if(should_install)
    if(APPLE)
      install(TARGETS ${exe} BUNDLE DESTINATION "${CMAKE_BINARY_DIR}" COMPONENT lokinet)
    else()
      install(TARGETS ${exe} RUNTIME DESTINATION bin COMPONENT lokinet)
    endif()
  endif()
endforeach()

if(APPLE)
#  add_custom_command(TARGET lokinet
#    POST_BUILD
#    COMMAND ${CMAKE_COMMAND} -E echo "setting rpath"
#    COMMAND ${CMAKE_INSTALL_NAME_TOOL} -add_rpath "@executable_path/../Frameworks/" $<TARGET_FILE:lokinet>
#    )
  add_custom_target(icons ALL
    COMMAND ${PROJECT_SOURCE_DIR}/contrib/macos/mk-icns.sh ${PROJECT_SOURCE_DIR}/contrib/lokinet.svg ${CMAKE_CURRENT_BINARY_DIR}/lokinet.icns
    DEPENDS ${PROJECT_SOURCE_DIR}/contrib/lokinet.svg ${PROJECT_SOURCE_DIR}/contrib/macos/mk-icns.sh)
  add_dependencies(lokinet icons lokinet-extension)
  install(TARGETS lokinet-extension FRAMEWORK DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/Lokinet.app/Contents/Frameworks" COMPONENT lokinet)
  set_target_properties(lokinet
    PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_INFO_STRING "Lokinet IP Packet Onion Router"
    MACOSX_BUNDLE_BUNDLE_NAME "Lokinet"
    MACOSX_BUNDLE_BUNDLE_VERSION "${lokinet_VERSION_MAJOR}.${lokinet_VERSION_MINOR}.${lokinet_VERSION_PATCH}"
    MACOSX_BUNDLE_LONG_VERSION_STRING "${lokinet_VERSION}.$lokinet_VERSION_MINOR}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${lokinet_VERSION_MAJOR}.${lokinet_VERSION_MINOR}"
    MACOSX_BUNDLE_GUI_IDENTIFIER "org.lokinet.lokinet"
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/contrib/macos/Info.plist"
    MACOSX_BUNDLE_ICON_FILE "${CMAKE_CURRENT_BINARY_DIR}/lokinet.icns"
    MACOSX_BUNDLE_COPYRIGHT "© 2021, The Loki Project")
  option(CODESIGN_KEY "codesign all the shit with this key" OFF)
  if (CODESIGN_KEY)
    message(STATUS "codesigning with ${CODESIGN_KEY}")
    set(SIGN_TARGET "${CMAKE_CURRENT_BINARY_DIR}/Lokinet.app")
    set(SIGN_ENTITLEMENTS "${CMAKE_SOURCE_DIR}/contrib/macos/lokinet.entitlements.plist")
    configure_file(
      "${PROJECT_SOURCE_DIR}/contrib/macos/sign.sh.in"
      "${CMAKE_BINARY_DIR}/sign.sh")
    add_custom_target(
      sign
      DEPENDS "${CMAKE_BINARY_DIR}/sign.sh" lokinet lokinet-extension
      COMMAND "${CMAKE_BINARY_DIR}/sign.sh"
      )
  else()
    message(STATUS "will not codesign")
  endif()
endif()

if(SETCAP)
  install(CODE "execute_process(COMMAND ${SETCAP} cap_net_admin,cap_net_bind_service=+eip ${CMAKE_INSTALL_PREFIX}/bin/lokinet)")
endif()
