
set(default_build_gui OFF)
set(default_gui_target pack)
if(APPLE)
  set(default_build_gui ON)
  set(default_gui_target macos:raw)
elseif(WIN32)
  set(default_build_gui ON)
  set(default_gui_target win32)
endif()

option(BUILD_GUI "build electron gui from 'gui' submodule source" ${default_build_gui})
set(GUI_YARN_TARGET "${default_gui_target}" CACHE STRING "yarn target for building the GUI")
set(GUI_YARN_EXTRA_OPTS "" CACHE STRING "extra options to pass into the yarn build command")

if (BUILD_GUI)
  message(STATUS "Building lokinet-gui")

  find_program(YARN NAMES yarn yarnpkg REQUIRED)
  message(STATUS "Building lokinet-gui with yarn ${YARN}, target ${GUI_YARN_TARGET}")
  set(wine_env)
  if(WIN32)
    set(wine_env WINEDEBUG=-all "WINEPREFIX=${PROJECT_BINARY_DIR}/wineprefix")
  endif()

  add_custom_target(lokinet-gui
    COMMAND ${YARN} install --frozen-lockfile &&
      ${wine_env} ${YARN} ${GUI_YARN_EXTRA_OPTS} ${GUI_YARN_TARGET}
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}/gui")

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
    add_custom_target(copy_gui ALL
      DEPENDS lokinet lokinet-gui
      # FIXME: we really shouldn't be building inside the source directory but this is npm...
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${PROJECT_SOURCE_DIR}/gui/release/Lokinet-GUI_portable.exe"
        "${PROJECT_BINARY_DIR}/gui/lokinet-gui.exe"
    )
  else()
    message(FATAL_ERROR "Building/bundling the GUI from this repository is not supported on this platform")
  endif()

else()
  message(STATUS "Not building lokinet-gui")
endif()
