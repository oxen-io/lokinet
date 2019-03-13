set(TEST_EXE testAll)
set(GTEST_DIR gtest)
add_subdirectory(${GTEST_DIR})

list(APPEND TEST_SRC
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
    net/test_llarp_net_inaddr.cpp
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
    util/test_llarp_util_aligned.cpp
    util/test_llarp_util_bencode.cpp
    util/test_llarp_util_bits.cpp
    util/test_llarp_util_encode.cpp
    util/test_llarp_util_ini.cpp
    util/test_llarp_util_printer.cpp
    util/test_llarp_util_queue_manager.cpp
    util/test_llarp_util_queue.cpp
    util/test_llarp_util_thread_pool.cpp
    util/test_llarp_util_traits.cpp
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

target_link_libraries(${TEST_EXE} PUBLIC gmock gtest ${STATIC_LIB})
target_include_directories(${TEST_EXE} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

if(NOT WIN32)
    target_link_libraries(${TEST_EXE} PUBLIC absl::variant)
else()
    target_sources(${TEST_EXE} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/win32/test.rc")
    target_link_libraries(${TEST_EXE} PUBLIC ws2_32 iphlpapi)
endif(NOT WIN32)
