set(default_build_gui OFF)
set(default_gui_target pack)
if(APPLE)
  set(default_build_gui ON)
  set(default_gui_target macos:raw)
elseif(WIN32)
  set(default_build_gui ON)
  set(default_gui_target win32)
endif()

if(WIN32)
  option(GUI_EXE "path to an externally built lokinet gui.exe" OFF)
endif()

option(BUILD_GUI "build electron gui from 'gui' submodule source" ${default_build_gui})
set(GUI_YARN_TARGET "${default_gui_target}" CACHE STRING "yarn target for building the GUI")
set(GUI_YARN_EXTRA_OPTS "" CACHE STRING "extra options to pass into the yarn build command")


if (BUILD_GUI)
  message(STATUS "Building lokinet-gui")
  # allow manually specifying yarn with -DYARN=
  if(NOT YARN)
    find_program(YARN NAMES yarnpkg yarn REQUIRED)
  endif()
  message(STATUS "Building lokinet-gui with yarn ${YARN}, target ${GUI_YARN_TARGET}")


  if(NOT WIN32)
    add_custom_target(lokinet-gui
      COMMAND ${YARN} install --frozen-lockfile &&
      ${YARN} ${GUI_YARN_EXTRA_OPTS} ${GUI_YARN_TARGET}
      WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}/gui")
  endif()

  if(APPLE)
    add_custom_target(assemble_gui ALL
      DEPENDS assemble lokinet-gui
      COMMAND mkdir "${lokinet_app}/Contents/Helpers"
      COMMAND cp -a "${PROJECT_SOURCE_DIR}/gui/release/mac/Lokinet-GUI.app" "${lokinet_app}/Contents/Helpers/"
      COMMAND mkdir -p "${lokinet_app}/Contents/Resources/en.lproj"
      COMMAND cp "${PROJECT_SOURCE_DIR}/contrib/macos/InfoPlist.strings" "${lokinet_app}/Contents/Resources/en.lproj/"
      COMMAND cp "${lokinet_app}/Contents/Resources/icon.icns" "${lokinet_app}/Contents/Helpers/Lokinet-GUI.app/Contents/Resources/icon.icns"
      COMMAND cp "${PROJECT_SOURCE_DIR}/contrib/macos/InfoPlist.strings" "${lokinet_app}/Contents/Helpers/Lokinet-GUI.app/Contents/Resources/en.lproj/"
      COMMAND /usr/libexec/PlistBuddy
      -c "Delete :CFBundleDisplayName"
      -c "Add :LSHasLocalizedDisplayName bool true"
      -c "Add :CFBundleDevelopmentRegion string en"
      -c "Set :CFBundleShortVersionString ${lokinet_VERSION}"
      -c "Set :CFBundleVersion ${lokinet_VERSION}.${LOKINET_APPLE_BUILD}"
      "${lokinet_app}/Contents/Helpers/Lokinet-GUI.app/Contents/Info.plist"
    )
  elseif(WIN32)
    file(MAKE_DIRECTORY "${PROJECT_BINARY_DIR}/gui")
    option(GUI_ZIP_FILE "custom lokinet gui for windows from zip file" OFF)
    if(GUI_ZIP_FILE)
      message(STATUS "using custom lokinet gui from ${GUI_ZIP_FILE}")
      execute_process(COMMAND ${CMAKE_COMMAND} -E tar xf ${GUI_ZIP_FILE}
        WORKING_DIRECTORY ${PROJECT_BINARY_DIR})
      add_custom_target("${PROJECT_BINARY_DIR}/gui/lokinet-gui.exe" COMMAND "true")
    elseif(GUI_EXE)
      message(STATUS "using custom lokinet gui executable: ${GUI_EXE}")
      execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different "${GUI_EXE}" "${PROJECT_BINARY_DIR}/gui/lokinet-gui.exe")
      add_custom_target("${PROJECT_BINARY_DIR}/gui/lokinet-gui.exe" COMMAND "true")
    else()
      add_custom_command(OUTPUT "${PROJECT_BINARY_DIR}/gui/lokinet-gui.exe"
        COMMAND ${YARN} install --frozen-lockfile &&
        USE_SYSTEM_7ZA=true DISPLAY= WINEDEBUG=-all WINEPREFIX="${PROJECT_BINARY_DIR}/wineprefix" ${YARN} ${GUI_YARN_EXTRA_OPTS} ${GUI_YARN_TARGET}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${PROJECT_SOURCE_DIR}/gui/release/Lokinet-GUI_portable.exe"
        "${PROJECT_BINARY_DIR}/gui/lokinet-gui.exe"
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}/gui")
    endif()
    add_custom_target(assemble_gui ALL COMMAND "true" DEPENDS "${PROJECT_BINARY_DIR}/gui/lokinet-gui.exe")
  else()
    message(FATAL_ERROR "Building/bundling the GUI from this repository is not supported on this platform")
  endif()
else()
  message(STATUS "not building gui")
endif()

if(NOT TARGET assemble_gui)
  add_custom_target(assemble_gui COMMAND "true")
endif()
