# We do this via a custom command that re-invokes a cmake script because we need the DEPENDS on .git/index so that we will re-run it (to regenerate the commit tag in the version) whenever the current commit changes. If we used a configure_file directly here, it would only re-run when something else causes cmake to re-run.

set(VERSIONTAG "${GIT_VERSION}")
set(GIT_INDEX_FILE "${PROJECT_SOURCE_DIR}/.git/index")
if(EXISTS ${GIT_INDEX_FILE} AND ( GIT_FOUND OR Git_FOUND) )
  message(STATUS "Found Git: ${GIT_EXECUTABLE}")
  add_custom_command(
    OUTPUT            "${CMAKE_CURRENT_BINARY_DIR}/constants/version.cpp"
    COMMAND           "${CMAKE_COMMAND}"
                      "-D" "GIT=${GIT_EXECUTABLE}"
                      "-D" "SRC=${CMAKE_CURRENT_SOURCE_DIR}/constants/version.cpp.in"
                      "-D" "DEST=${CMAKE_CURRENT_BINARY_DIR}/constants/version.cpp"
                      "-P" "${CMAKE_CURRENT_LIST_DIR}/GenVersion.cmake"
    DEPENDS           "${CMAKE_CURRENT_SOURCE_DIR}/constants/version.cpp.in"
                      "${GIT_INDEX_FILE}")
  if(WIN32)
    foreach(exe IN ITEMS lokinet lokinet-vpn lokinet-bootstrap)
      set(lokinet_EXE_NAME "${exe}.exe")
      add_custom_command(
        OUTPUT            "${CMAKE_BINARY_DIR}/${exe}.rc"
        COMMAND           "${CMAKE_COMMAND}"
        "-D" "GIT=${GIT_EXECUTABLE}"
        "-D" "SRC=${CMAKE_CURRENT_SOURCE_DIR}/win32/version.rc.in"
        "-D" "DEST=${CMAKE_BINARY_DIR}/${exe}.rc"
        "-P" "${CMAKE_CURRENT_LIST_DIR}/GenVersion.cmake"
        DEPENDS           "${CMAKE_CURRENT_SOURCE_DIR}/win32/version.rc.in"
        "${GIT_INDEX_FILE}")
    endforeach()
  endif()
else()
  configure_file("${CMAKE_CURRENT_SOURCE_DIR}/constants/version.cpp.in" "${CMAKE_CURRENT_BINARY_DIR}/constants/version.cpp" @ONLY)
  if(WIN32)
    foreach(exe IN ITEMS lokinet lokinet-vpn lokinet-bootstrap)
      set(lokinet_EXE_NAME "${exe}.exe")
      configure_file("${CMAKE_CURRENT_SOURCE_DIR}/win32/version.rc.in" "${CMAKE_BINARY_DIR}/${exe}.rc" @ONLY)
    endforeach()
  endif()
endif()

add_custom_target(genversion_cpp DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/constants/version.cpp")
if(WIN32)
  add_custom_target(genversion_rc DEPENDS "${CMAKE_BINARY_DIR}/lokinet.rc" "${CMAKE_BINARY_DIR}/lokinet-vpn.rc" "${CMAKE_BINARY_DIR}/lokinet-bootstrap.rc")
else()
  add_custom_target(genversion_rc)
endif()
add_custom_target(genversion DEPENDS genversion_cpp genversion_rc)
