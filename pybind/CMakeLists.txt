if(NOT BUILD_LIBLOKINET)
  message(FATAL_ERROR "pybind requires building liblokinet")
endif()
if(USE_JEMALLOC)
  message(FATAL_ERROR "pybind cannot be built with jemalloc enabled")
endif()

pybind11_add_module(pylokinet MODULE
  module.cpp
  lokinet/context.cpp
  lokinet/socket.cpp
)
target_link_libraries(pylokinet PUBLIC lokinet-shared)
target_include_directories(pylokinet PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
add_log_tag(pylokinet)

if (WITH_TESTS)
  add_custom_target(pylokinet_build DEPENDS liblokinet pylokinet)
  add_custom_target(pytest ${CMAKE_COMMAND} -E
      env PYTHONPATH="$ENV{PYTHONPATH}:${CMAKE_BINARY_DIR}/pybind"
      ${PYTHON_EXECUTABLE} -m pytest
      ${CMAKE_CURRENT_SOURCE_DIR}
      DEPENDS
      pylokinet_build)
endif()
