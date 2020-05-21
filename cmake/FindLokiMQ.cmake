find_package(PkgConfig)
if (PKG_CONFIG_FOUND)
  pkg_check_modules(PC_LOKIMQ QUIET liblokimq>=${LokiMQ_VERSION})
endif()

find_path(LOKIMQ_INCLUDE_DIR lokimq/lokimq.h
  HINTS ${PC_LOKIMQ_INCLUDEDIR} ${PC_LOKIMQ_INCLUDE_DIRS})

find_library(LOKIMQ_LIBRARY NAMES lokimq
  HINTS ${PC_LOKIMQ_LIBDIR} ${PC_LOKIMQ_LIBRARY_DIRS})

mark_as_advanced(LOKIMQ_INCLUDE_DIR LOKIMQ_LIBRARY)

set(LOKIMQ_LIBRARIES ${LOKIMQ_LIBRARY} ${PC_LOKIMQ_LIBRARIES})
set(LOKIMQ_INCLUDE_DIRS ${LOKIMQ_INCLUDE_DIR})

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set LIBUV_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(LokiMQ DEFAULT_MSG
                                  LOKIMQ_LIBRARY LOKIMQ_INCLUDE_DIR)

mark_as_advanced(LOKIMQ_INCLUDE_DIR LOKIMQ_LIBRARY)
