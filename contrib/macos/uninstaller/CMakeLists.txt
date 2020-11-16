cmake_minimum_required(VERSION 3.10)  # bionic's cmake version

# Has to be set before `project()`, and ignored on non-macos:
set(CMAKE_OSX_DEPLOYMENT_TARGET 10.12 CACHE STRING "macOS deployment target (Apple clang only)")

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
  foreach(lang C CXX)
    if(NOT DEFINED CMAKE_${lang}_COMPILER_LAUNCHER AND NOT CMAKE_${lang}_COMPILER MATCHES ".*/ccache")
      message(STATUS "Enabling ccache for ${lang}")
      set(CMAKE_${lang}_COMPILER_LAUNCHER ${CCACHE_PROGRAM} CACHE STRING "")
    endif()
  endforeach()
endif()

set(PROJECT_NAME lokinet-uninstaller)
project(${PROJECT_NAME}
    VERSION 0.0.1
    DESCRIPTION "lokinet uninstaller for macos"
    LANGUAGES CXX)

add_executable(${PROJECT_NAME}
  main.cpp)

find_package(Qt5 COMPONENTS Widgets REQUIRED)
  
target_link_libraries(${PROJECT_NAME} PRIVATE
    "-framework Security"
    Qt5::Core Qt5::Widgets)

set_target_properties(${PROJECT_NAME}
    PROPERTIES
        CXX_STANDARD 17
        CXX_EXTENSIONS OFF
        CXX_STANDARD_REQUIRED ON
    )

  
set(MACOS_SIGN ""
  CACHE STRING "enable codesigning -- use a 'Apple Distribution' key (or key description) from `security find-identity -v`")

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/lokinet-uninstall.icns
  COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/mk-icns.sh ${CMAKE_CURRENT_SOURCE_DIR}/icon.svg ${CMAKE_CURRENT_BINARY_DIR}/lokinet-uninstall.icns
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/icon.svg ${CMAKE_CURRENT_SOURCE_DIR}/mk-icns.sh)

target_sources(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/lokinet-uninstall.icns)

set_target_properties(${PROJECT_NAME}
  PROPERTIES
  MACOSX_BUNDLE TRUE
  OUTPUT_NAME UninstallLokinet
  RESOURCE "${CMAKE_CURRENT_BINARY_DIR}/lokinet-uninstall.icns")

set(MACOSX_BUNDLE_BUNDLE_NAME UninstallLokinet)
set(MACOSX_BUNDLE_GUI_IDENTIFIER org.lokinet.lokinet-uninstaller)
set(MACOSX_BUNDLE_INFO_STRING "Lokinet uninstaller")
set(MACOSX_BUNDLE_ICON_FILE lokinet-uninstall.icns)
set(MACOSX_BUNDLE_LONG_VERSION_STRING ${PROJECT_VERSION})
set(MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION})
set(MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION})
set(MACOSX_BUNDLE_COPYRIGHT "© 2020, The Loki Project")

get_target_property(uic_location Qt5::uic IMPORTED_LOCATION)
get_filename_component(qt_dir ${uic_location} DIRECTORY)
 
if(MACOS_SIGN)
  add_custom_command(TARGET ${PROJECT_NAME}
    POST_BUILD
    COMMAND echo "Running qt magic macos deploy script"
    COMMAND "${qt_dir}/macdeployqt" UninstallLokinet.app -always-overwrite
    COMMAND echo "Signing app bundle and everything inside it"
    COMMAND codesign -s "${MACOS_SIGN}" --deep --strict --options runtime
    --entitlements ${PROJECT_SOURCE_DIR}/lokinet-uninstaller-entitlements.plist --force -vvv UninstallLokinet.app
    )
else()
  add_custom_command(TARGET ${PROJECT_NAME}
    POST_BUILD
    COMMAND echo "Running qt magic macos deploy script"
    COMMAND "${qt_dir}/macdeployqt" UninstallLokinet.app -always-overwrite
    )
endif()

install(TARGETS lokinet-uninstaller
    RUNTIME DESTINATION bin
    BUNDLE DESTINATION .
    RESOURCE DESTINATION .)
