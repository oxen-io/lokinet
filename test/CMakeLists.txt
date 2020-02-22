set(GTEST_EXE testAll)
set(CATCH_EXE catchAll)

# Old gtest-based tests; new tests should use Catch2, instead.
list(APPEND GTEST_SRC
    config/test_llarp_config_config.cpp
    config/test_llarp_config_ini.cpp
    crypto/test_llarp_crypto_types.cpp
    crypto/test_llarp_crypto.cpp
    crypto/test_llarp_key_manager.cpp
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
    router/test_llarp_router_version.cpp
    routing/llarp_routing_transfer_traffic.cpp
    routing/test_llarp_routing_obtainexitmessage.cpp
    service/test_llarp_service_address.cpp
    service/test_llarp_service_identity.cpp
    test_libabyss.cpp
    test_llarp_encrypted_frame.cpp
    test_llarp_router_contact.cpp
    test_md5.cpp
    util/meta/test_llarp_util_memfn.cpp
    util/meta/test_llarp_util_traits.cpp
    util/test_llarp_util_aligned.cpp
    util/test_llarp_util_bencode.cpp
    util/test_llarp_util_bits.cpp
    util/test_llarp_util_decaying_hashset.cpp
    util/test_llarp_util_encode.cpp
    util/test_llarp_util_printer.cpp
    util/test_llarp_util_log_level.cpp
    util/thread/test_llarp_util_queue_manager.cpp
    util/thread/test_llarp_util_queue.cpp
    util/thread/test_llarp_util_thread_pool.cpp
)

add_executable(${GTEST_EXE}
    # helpers
    main.cpp
    crypto/mock_crypto.cpp
    dht/mock_context.cpp
    test_util.cpp
    # actual test cases
    ${GTEST_SRC}
)

target_link_libraries(${GTEST_EXE} PUBLIC gmock gtest ${STATIC_LIB})
target_include_directories(${GTEST_EXE} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

if(NOT WIN32)
    target_link_libraries(${GTEST_EXE} PUBLIC absl::variant)
else()
    target_sources(${GTEST_EXE} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/win32/test.rc")
    target_link_libraries(${GTEST_EXE} PUBLIC ws2_32 iphlpapi shlwapi)
endif(NOT WIN32)

if(${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")
  target_link_directories(${GTEST_EXE} PRIVATE /usr/local/lib)
endif()

add_subdirectory(Catch2)

add_executable(${CATCH_EXE}
  nodedb/test_nodedb.cpp
  path/test_path.cpp
  util/test_llarp_util_str.cpp
  check_main.cpp)

target_link_libraries(${CATCH_EXE} PUBLIC ${STATIC_LIB} Catch2::Catch2)
target_include_directories(${CATCH_EXE} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Custom targets to invoke the different test suites:
add_custom_target(catch COMMAND ${CATCH_EXE})
add_custom_target(rungtest COMMAND ${GTEST_EXE})

# Add a custom "check" target that runs all the test suites:
add_custom_target(check DEPENDS rungtest catch)
