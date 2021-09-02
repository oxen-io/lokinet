set(CPACK_PACKAGE_VENDOR "lokinet.org")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://lokinet.org/")
set(CPACK_RESOURCE_FILE_README "${PROJECT_SOURCE_DIR}/contrib/readme-installer.txt")
set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/LICENSE.txt")

if(WIN32)
  include(cmake/win32_installer_deps.cmake)
endif()


# This must always be last!
include(CPack)

