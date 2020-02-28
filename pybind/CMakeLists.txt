
set(PYTHON_EXECUTABLE "python3")
set(LLARP_PYBIND_SRC
  module.cpp
  llarp/context.cpp
  llarp/router_contact.cpp
  llarp/crypto/types.cpp
  llarp/config.cpp
  llarp/tooling/router_hive.cpp
  llarp/tooling/router_event.cpp
)

pybind11_add_module(pyllarp MODULE ${LLARP_PYBIND_SRC})
target_include_directories(pyllarp PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/crypto/libntrup/include ${CMAKE_SOURCE_DIR}/llarp ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pyllarp PUBLIC ${EXE_LIBS})

