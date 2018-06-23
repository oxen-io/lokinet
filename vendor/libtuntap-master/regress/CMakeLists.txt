# libtuntap regression tests CMakeLists.txt
# =========================================

cmake_minimum_required(VERSION 2.8)

enable_testing()
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
link_directories(${CMAKE_LIBRARY_OUTPUT_DIRECTORY})

# C based tests
# -------------
file(GLOB ALL_C_TESTS regress/test*.c)
foreach(SOURCE_FILE ${ALL_C_TESTS})
  string(REPLACE ".c" "" PATH_WO_SUFFIX ${SOURCE_FILE})
  string(REPLACE ${CMAKE_CURRENT_SOURCE_DIR}/ ""
    TEST_SRC_PATH ${SOURCE_FILE})
  string(REPLACE ${CMAKE_CURRENT_SOURCE_DIR}/regress/ ""
    TEST_NAME ${PATH_WO_SUFFIX})

  add_executable(${TEST_NAME} ${TEST_SRC_PATH})
  target_link_libraries(${TEST_NAME} tuntap)
  add_test(${TEST_NAME} ${EXECUTABLE_OUTPUT_PATH}/${TEST_NAME})
endforeach(SOURCE_FILE)

# Shell based tests
# -----------------
file(GLOB ALL_SH_TESTS regress/test*.sh)
foreach(SOURCE_FILE ${ALL_SH_TESTS})
  string(REPLACE ".sh" "" PATH_WO_SUFFIX ${SOURCE_FILE})
  string(REPLACE ${CMAKE_CURRENT_SOURCE_DIR}/ ""
    TEST_SRC_PATH ${SOURCE_FILE})
  string(REPLACE ${CMAKE_CURRENT_SOURCE_DIR}/regress/ ""
    TEST_NAME ${PATH_WO_SUFFIX})
  
  string(REPLACE ".sh" ".c" HELPER_SRC_PATH ${TEST_SRC_PATH})
  string(REPLACE "test" "helper" HELPER_SRC_PATH ${HELPER_SRC_PATH})
  string(REPLACE "test" "helper" HELPER_NAME ${TEST_NAME})

  # XXX: Do it conditionaly
  add_executable(${HELPER_NAME} ${HELPER_SRC_PATH})
  target_link_libraries(${HELPER_NAME} tuntap)

  # XXX: This is cancerous
  file(REMOVE ${CMAKE_CURRENT_BINARY_DIR}/${TEST_NAME})
  file(COPY ${TEST_SRC_PATH}
       DESTINATION ${CMAKE_CURRENT_BINARY_DIR}
       FILE_PERMISSIONS OWNER_READ OWNER_EXECUTE
  )
  file(RENAME ${CMAKE_CURRENT_BINARY_DIR}/${TEST_NAME}.sh
    ${CMAKE_CURRENT_BINARY_DIR}/${TEST_NAME})

  add_test(${TEST_NAME} ${EXECUTABLE_OUTPUT_PATH}/${TEST_NAME})
endforeach(SOURCE_FILE)

# "Will fail" tests
# -----------------

# Only NetBSD, FreeBSD and DragonFlyBSD have the debug mode compiled by default
if(NOT NetBSD AND NOT FreeBSD AND NOT DragonFly)
  set_tests_properties(test39 PROPERTIES WILL_FAIL true)
  set_tests_properties(test40 PROPERTIES WILL_FAIL true)
endif()

# Only Linux has a tuntap_set_ifname() implemented
if(NOT Linux)
  set_tests_properties(test41 PROPERTIES WILL_FAIL true)
endif()

# Only FreeBSD and OpenBSD have a tuntap_set_descr() implemented
if(NOT FreeBSD AND NOT OpenBSD)
  set_tests_properties(test44 PROPERTIES WILL_FAIL true)
  set_tests_properties(test46 PROPERTIES WILL_FAIL true)
endif()

# Windows work-in-progress (tap)
if (Windows)
  set_tests_properties(test05 PROPERTIES WILL_FAIL true)
  set_tests_properties(test07 PROPERTIES WILL_FAIL true)
  set_tests_properties(test09 PROPERTIES WILL_FAIL true)
  set_tests_properties(test39 PROPERTIES WILL_FAIL true)
  set_tests_properties(test41 PROPERTIES WILL_FAIL true)
  set_tests_properties(test44 PROPERTIES WILL_FAIL true)
endif()

# Windows work-in-progress (tun)
if (Windows)
  set_tests_properties(test02 PROPERTIES WILL_FAIL true)
  set_tests_properties(test04 PROPERTIES WILL_FAIL true)
  set_tests_properties(test06 PROPERTIES WILL_FAIL true)
  set_tests_properties(test08 PROPERTIES WILL_FAIL true)
  set_tests_properties(test10 PROPERTIES WILL_FAIL true)
  set_tests_properties(test12 PROPERTIES WILL_FAIL true)
  set_tests_properties(test14 PROPERTIES WILL_FAIL true)
  set_tests_properties(test18 PROPERTIES WILL_FAIL true)
  set_tests_properties(test22 PROPERTIES WILL_FAIL true)
  set_tests_properties(test24 PROPERTIES WILL_FAIL true)
  set_tests_properties(test26 PROPERTIES WILL_FAIL true)
  set_tests_properties(test40 PROPERTIES WILL_FAIL true)
endif()

