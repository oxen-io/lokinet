pybind11_add_module(pyllarp MODULE
  module.cpp
  llarp/context.cpp
  llarp/router.cpp
  llarp/router_id.cpp
  llarp/router_contact.cpp
  llarp/crypto/types.cpp
  llarp/config.cpp
  llarp/logger.cpp
  llarp/peerstats.cpp
  llarp/dht/dht_types.cpp
  llarp/path/path_types.cpp
  llarp/path/path_hop_config.cpp
  llarp/handlers/pyhandler.cpp
  llarp/tooling/router_hive.cpp
  llarp/tooling/router_event.cpp
  llarp/service/address.cpp
)
target_link_libraries(pyllarp PUBLIC lokinet-amalgum)
target_include_directories(pyllarp PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
