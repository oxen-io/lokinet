if (WITH_HIVE)
  add_custom_target(hive_build DEPENDS lokinet-amalgum pyllarp)
  add_custom_target(hive ${CMAKE_COMMAND} -E
      env PYTHONPATH="$ENV{PYTHONPATH}:${CMAKE_BINARY_DIR}/pybind"
      ${PYTHON_EXECUTABLE} -m pytest
      ${CMAKE_CURRENT_SOURCE_DIR}/hive
      DEPENDS
      hive_build)
endif()

add_subdirectory(Catch2)
add_executable(testAll
  # helpers
  check_main.cpp
  test_util.cpp
  # actual test cases
  config/test_llarp_config_definition.cpp
  config/test_llarp_config_ini.cpp
  config/test_llarp_config_output.cpp
  config/test_llarp_config_values.cpp
  crypto/test_llarp_crypto_types.cpp
  crypto/test_llarp_crypto.cpp
  crypto/test_llarp_key_manager.cpp
  dns/test_llarp_dns_dns.cpp
  net/test_ip_address.cpp
  net/test_llarp_net.cpp
  net/test_sock_addr.cpp
  nodedb/test_nodedb.cpp
  path/test_path.cpp
  router/test_llarp_router_version.cpp
  routing/test_llarp_routing_transfer_traffic.cpp
  routing/test_llarp_routing_obtainexitmessage.cpp
  service/test_llarp_service_address.cpp
  service/test_llarp_service_identity.cpp
  service/test_llarp_service_name.cpp
  util/meta/test_llarp_util_memfn.cpp
  util/thread/test_llarp_util_queue_manager.cpp
  util/thread/test_llarp_util_queue.cpp
  util/test_llarp_util_aligned.cpp
  util/test_llarp_util_bencode.cpp
  util/test_llarp_util_bits.cpp
  util/test_llarp_util_decaying_hashset.cpp
  util/test_llarp_util_log_level.cpp
  util/test_llarp_util_str.cpp
  test_llarp_encrypted_frame.cpp
  test_llarp_router_contact.cpp)


if(WITH_PEERSTATS_BACKEND)
  target_sources(testAll PRIVATE
    peerstats/test_peer_db.cpp
    peerstats/test_peer_types.cpp)
endif()

target_link_libraries(testAll PUBLIC lokinet-amalgum Catch2::Catch2)
target_include_directories(testAll PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

if(WIN32)
    target_sources(testAll PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/win32/test.rc")
    target_link_libraries(testAll PUBLIC ws2_32 iphlpapi shlwapi)
endif()

add_custom_target(check COMMAND testAll)
