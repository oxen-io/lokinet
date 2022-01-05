option(SUBMODULE_CHECK "Enables checking that vendored library submodules are up to date" ON)
if(SUBMODULE_CHECK)
  find_package(Git)
  if(GIT_FOUND)
    function(check_submodule relative_path)
      execute_process(COMMAND git rev-parse "HEAD" WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${relative_path} OUTPUT_VARIABLE localHead)
      execute_process(COMMAND git rev-parse "HEAD:external/${relative_path}" WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} OUTPUT_VARIABLE checkedHead)
      string(COMPARE EQUAL "${localHead}" "${checkedHead}" upToDate)
      if (upToDate)
        message(STATUS "Submodule 'external/${relative_path}' is up-to-date")
      else()
        message(FATAL_ERROR "Submodule 'external/${relative_path}' is not up-to-date. Please update with\ngit submodule update --init --recursive\nor run cmake with -DSUBMODULE_CHECK=OFF")
      endif()
    endfunction ()

    message(STATUS "Checking submodules")
    check_submodule(nlohmann)
    check_submodule(cxxopts)
    check_submodule(ghc-filesystem)
    check_submodule(date)
    check_submodule(pybind11)
    check_submodule(sqlite_orm)
    check_submodule(oxen-mq)
    check_submodule(oxen-encoding)
    check_submodule(uvw)
    check_submodule(cpr)
    check_submodule(ngtcp2)
    if(WIN32)
      check_submodule(mullvad-windows-libraries)
      check_submodule(mullvad-libwfp)
    endif()
  endif()
endif()

if(WIN32)
  set(mullvad_wincommon_src
    libcommon/fileenumerator.cpp
    libcommon/process/process.cpp
    libcommon/process/applicationrunner.cpp
    libcommon/logging/logsink.cpp
    libcommon/registry/registrykey.cpp
    libcommon/registry/registry.cpp
    libcommon/registry/registrymonitor.cpp
    libcommon/registry/registrypath.cpp
    libcommon/filesystem.cpp
    libcommon/string.cpp
    libcommon/trace/consoletracesink.cpp
    libcommon/trace/trace.cpp
    libcommon/trace/filetracesink.cpp
    libcommon/serialization/deserializer.cpp
    libcommon/serialization/serializer.cpp
    libcommon/binarycomposer.cpp
    libcommon/security.cpp
    libcommon/guid.cpp
    libcommon/network.cpp
    libcommon/burstguard.cpp
    libcommon/error.cpp
    libcommon/resourcedata.cpp
    libcommon/stdafx.cpp
    libcommon/network/adapters.cpp)

  set(mullvad_wincommon_root mullvad-windows-libraries/src/)
  list(TRANSFORM mullvad_wincommon_src PREPEND ${mullvad_wincommon_root})

  set(mullvad_libwfp_src
    libwfp/conditionbuilder.cpp
    libwfp/sublayerbuilder.cpp
    libwfp/internal/conditionassembler.cpp
    libwfp/ipnetwork.cpp
    libwfp/filterbuilder.cpp
    libwfp/ipaddress.cpp
    libwfp/objectexplorer.cpp
    libwfp/objectinstaller.cpp
    libwfp/providerbuilder.cpp
    libwfp/filterengine.cpp
    libwfp/objectmonitor.cpp
    libwfp/objectenumerator.cpp
    libwfp/conditions/conditionip.cpp
    libwfp/conditions/conditionport.cpp
    libwfp/conditions/conditionportrange.cpp
    libwfp/conditions/conditionicmp.cpp
    libwfp/conditions/conditiondirection.cpp
    libwfp/conditions/conditionapplication.cpp
    libwfp/conditions/conditionprotocol.cpp
    libwfp/conditions/conditioninterface.cpp
    libwfp/conditions/conditionloopback.cpp
    libwfp/objectdeleter.cpp
    libwfp/transaction.cpp
    libwfp/stdafx.cpp
    libwfp/layerconditions.cpp)

  set(mullvad_libwfp_root mullvad-libwfp/src/)
  list(TRANSFORM mullvad_libwfp_src PREPEND ${mullvad_libwfp_root})

  add_library(mullvad_wfp ${mullvad_wincommon_src} ${mullvad_libwfp_src})
  target_include_directories(mullvad_wfp PUBLIC ${mullvad_wincommon_root} ${mullvad_libwfp_root})
endif()

if(WITH_HIVE)
  add_subdirectory(pybind11 EXCLUDE_FROM_ALL)
endif()

set(JSON_BuildTests OFF CACHE INTERNAL "")
add_subdirectory(nlohmann EXCLUDE_FROM_ALL)
add_subdirectory(cxxopts EXCLUDE_FROM_ALL)
add_subdirectory(date EXCLUDE_FROM_ALL)

add_library(sqlite_orm INTERFACE)
target_include_directories(sqlite_orm SYSTEM INTERFACE sqlite_orm/include)
if(NOT TARGET sqlite3)
  add_library(sqlite3 INTERFACE)
  pkg_check_modules(SQLITE3 REQUIRED IMPORTED_TARGET sqlite3)
  target_link_libraries(sqlite3 INTERFACE PkgConfig::SQLITE3)
endif()
target_link_libraries(sqlite_orm INTERFACE sqlite3)

add_library(uvw INTERFACE)
target_include_directories(uvw INTERFACE uvw/src)
target_link_libraries(uvw INTERFACE libuv)

# ngtcp2 needs some massaging to build nicely:
include(ngtcp2_lib)
add_ngtcp2_lib()

# cpr configuration.  Ideally we'd just do this via add_subdirectory, but cpr's cmake requires
# 3.15+, and we target lower than that (and this is fairly simple to build).
if(WITH_BOOTSTRAP)
  if(NOT BUILD_STATIC_DEPS)
    find_package(CURL REQUIRED COMPONENTS HTTP HTTPS SSL)

    # CURL::libcurl wasn't added to FindCURL until cmake 3.12, so add it if necessary
    if (CMAKE_VERSION VERSION_LESS 3.12 AND NOT TARGET CURL::libcurl)
      add_library(libcurl UNKNOWN IMPORTED GLOBAL)
      set_target_properties(libcurl PROPERTIES
        IMPORTED_LOCATION ${CURL_LIBRARIES}
        INTERFACE_INCLUDE_DIRECTORIES "${CURL_INCLUDE_DIRS}")
      add_library(CURL_libcurl INTERFACE)
      target_link_libraries(CURL_libcurl INTERFACE libcurl)
      add_library(CURL::libcurl ALIAS CURL_libcurl)
    endif()
  endif()

  file(GLOB cpr_sources ${conf_depends} cpr/cpr/*.cpp)

  add_library(cpr STATIC EXCLUDE_FROM_ALL ${cpr_sources})
  target_link_libraries(cpr PUBLIC CURL::libcurl)
  target_include_directories(cpr PUBLIC cpr/include)
  target_compile_definitions(cpr PUBLIC CPR_CURL_NOSIGNAL)
  add_library(cpr::cpr ALIAS cpr)
endif()
