
find_package(Git QUIET)
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
else()
  message(STATUS "Git was not found! Setting version to to nogit")
  set(VERSIONTAG "nogit")
  configure_file("${CMAKE_CURRENT_SOURCE_DIR}/constants/version.cpp.in" "${CMAKE_CURRENT_BINARY_DIR}/constants/version.cpp")
endif()

add_custom_target(genversion DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/constants/version.cpp")
