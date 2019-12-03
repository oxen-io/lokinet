set(TEST_EXE testAll)

list(APPEND TEST_SRC
    config/test_llarp_config_config.cpp
    config/test_llarp_config_ini.cpp
    crypto/test_llarp_crypto_types.cpp
    crypto/test_llarp_crypto.cpp
    dht/test_llarp_dht_bucket.cpp
    dht/test_llarp_dht_explorenetworkjob.cpp
    dht/test_llarp_dht_kademlia.cpp
    dht/test_llarp_dht_key.cpp
    dht/test_llarp_dht_node.cpp
    dht/test_llarp_dht_serviceaddresslookup.cpp
    dht/test_llarp_dht_taglookup.cpp
    dht/test_llarp_dht_tx.cpp
    dht/test_llarp_dht_txowner.cpp
    dns/test_llarp_dns_dns.cpp
    exit/test_llarp_exit_context.cpp
    link/test_llarp_link.cpp
    llarp_test.cpp
    net/test_llarp_net.cpp
    routing/llarp_routing_transfer_traffic.cpp
    routing/test_llarp_routing_obtainexitmessage.cpp
    service/test_llarp_service_address.cpp
    service/test_llarp_service_identity.cpp
    test_libabyss.cpp
    test_llarp_dns.cpp
    test_llarp_dnsd.cpp
    test_llarp_encrypted_frame.cpp
    test_llarp_router_contact.cpp
    test_llarp_router.cpp
    test_md5.cpp
    util/meta/test_llarp_util_memfn.cpp
    util/meta/test_llarp_util_object.cpp
    util/meta/test_llarp_util_traits.cpp
    util/metrics/test_llarp_metrics_metricktank.cpp
    util/metrics/test_llarp_metrics_publisher.cpp
    util/metrics/test_llarp_util_metrics_core.cpp
    util/metrics/test_llarp_util_metrics_types.cpp
    util/test_llarp_util_aligned.cpp
    util/test_llarp_util_bencode.cpp
    util/test_llarp_util_bits.cpp
    util/test_llarp_util_encode.cpp
    util/test_llarp_util_printer.cpp
    util/test_llarp_utils_str.cpp
    util/thread/test_llarp_util_queue_manager.cpp
    util/thread/test_llarp_util_queue.cpp
    util/thread/test_llarp_util_thread_pool.cpp
    util/thread/test_llarp_util_timerqueue.cpp
    util/thread/test_llarp_utils_scheduler.cpp
)

add_executable(${TEST_EXE}
    # helpers
    main.cpp
    crypto/mock_crypto.cpp
    dht/mock_context.cpp
    test_util.cpp
    # actual test cases
    ${TEST_SRC}
)

target_link_libraries(${TEST_EXE} PUBLIC gmock gtest curl ${STATIC_LIB})
target_include_directories(${TEST_EXE} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

if(NOT WIN32)
    target_link_libraries(${TEST_EXE} PUBLIC absl::variant)
else()
    target_sources(${TEST_EXE} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/win32/test.rc")
    target_link_libraries(${TEST_EXE} PUBLIC ws2_32 iphlpapi shlwapi)
endif(NOT WIN32)

if(${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")
  target_link_directories(${TEST_EXE} PRIVATE /usr/local/lib)
endif()
