set(LLARP_PYBIND_SRC
  module.cpp
  llarp/context.cpp
  llarp/router_id.cpp
  llarp/router_contact.cpp
  llarp/crypto/types.cpp
  llarp/config.cpp
  llarp/path/path_types.cpp
  llarp/path/path_hop_config.cpp
  llarp/handlers/pyhandler.cpp
  llarp/tooling/router_hive.cpp
  llarp/tooling/router_event.cpp
  llarp/service/address.cpp
)

pybind11_add_module(pyllarp MODULE ${LLARP_PYBIND_SRC})
target_include_directories(pyllarp PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/crypto/libntrup/include ${CMAKE_SOURCE_DIR}/llarp ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pyllarp PUBLIC ${EXE_LIBS})

