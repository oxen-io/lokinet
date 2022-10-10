set(default_build_gui OFF)
if(APPLE OR WIN32)
  set(default_build_gui ON)
endif()

option(BUILD_GUI "build electron gui from 'gui' submodule source" ${default_build_gui})
