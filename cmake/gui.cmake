
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
  add_custom_target(lokinet-gui
    COMMAND ${YARN} install --frozen-lockfile &&
      WINEDEBUG=-all "WINEPREFIX=${PROJECT_BINARY_DIR}/wineprefix" ${YARN} ${GUI_YARN_EXTRA_OPTS} ${GUI_YARN_TARGET}
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}/gui")

  if(APPLE)
    add_custom_target(copy_gui ALL
      DEPENDS lokinet lokinet-extension lokinet-gui
      # FIXME: we really shouldn't be building inside the source directory but this is npm...
      COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${PROJECT_SOURCE_DIR}/lokinet-gui/release/mac/lokinet-gui.app
        $<TARGET_BUNDLE_DIR:lokinet>
    )
    add_dependencies(assemble copy_gui)
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
