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
