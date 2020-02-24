include(Version)

set(LIB_UTIL_SRC
  config/config.cpp
  config/ini.cpp
  config/key_manager.cpp
  constants/limits.cpp
  ${CMAKE_CURRENT_BINARY_DIR}/constants/version.cpp
  util/bencode.cpp
  util/buffer.cpp
  util/encode.cpp
  util/fs.cpp
  util/json.cpp
  util/logging/android_logger.cpp
  util/logging/file_logger.cpp
  util/logging/json_logger.cpp
  util/logging/logger.cpp
  util/logging/loglevel.cpp
  util/logging/ostream_logger.cpp
  util/logging/syslog_logger.cpp
  util/logging/win32_logger.cpp
  util/lokinet_init.c
  util/mem.cpp
  util/printer.cpp
  util/str.cpp
  util/thread/logic.cpp
  util/thread/queue_manager.cpp
  util/thread/thread_pool.cpp
  util/thread/threading.cpp
  util/thread/threadpool.cpp
  util/time.cpp
)

add_library(${UTIL_LIB} STATIC ${LIB_UTIL_SRC})

add_dependencies(${UTIL_LIB} genversion)

target_include_directories(${UTIL_LIB} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/include ${CURL_INCLUDE_DIRS})
if(ANDROID)
  set(LOG_LIB log)
endif()

target_link_libraries(${UTIL_LIB} PUBLIC ${CRYPTOGRAPHY_LIB} ${LOG_LIB} ${CURL_LIBRARIES})
target_link_libraries(${UTIL_LIB} PUBLIC
    absl::time absl::hash
    nlohmann_json::nlohmann_json
    ghc_filesystem
    optional-lite
    )

# cut back on fluff
if (NOT WIN32)
  target_link_libraries(${UTIL_LIB} PUBLIC absl::strings)
endif(NOT WIN32)

if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
  set(ISOLATE_PROC_SRC linux/netns.cpp)
endif()

set(LIB_PLATFORM_SRC
# for networking
  ev/ev.cpp
  ev/pipe.cpp
  ev/vpnio.cpp
  ev/ev_libuv.cpp
  net/ip.cpp
  net/net.cpp
  net/net_addr.cpp
  net/net_int.cpp
# for android shim
  ${ANDROID_PLATFORM_SRC}
# process isolation implementation
  ${ISOLATE_PROC_SRC}
# tun
  ${LIBTUNTAP_SRC}
  ${EV_SRC}
)

if (WIN32)
  set(LIB_PLATFORM_SRC
    ev/ev_win32.cpp
    ${LIB_PLATFORM_SRC}
    win32/win32_inet.c
    win32/win32_intrnl.c)
endif(WIN32)

add_library(${PLATFORM_LIB} STATIC ${LIB_PLATFORM_SRC})
target_link_libraries(${PLATFORM_LIB} PUBLIC ${CRYPTOGRAPHY_LIB} ${UTIL_LIB} Threads::Threads ${LIBS})

if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
  if(NON_PC_TARGET)
    add_import_library(rt)
    target_link_libraries(${PLATFORM_LIB} PUBLIC rt)
  endif()
endif()

if(WIN32)
  target_link_libraries(${PLATFORM_LIB} PUBLIC iphlpapi)
endif()
set(DNSLIB_SRC
  dns/message.cpp
  dns/name.cpp
  dns/question.cpp
  dns/rr.cpp
  dns/serialize.cpp
  dns/server.cpp
)

set(CONSENSUS_SRC
  consensus/table.cpp
)

set(LIB_SRC
  ${CONSENSUS_SRC}
  ${DNSLIB_SRC}
  bootstrap.cpp
  context.cpp
  crypto/crypto_libsodium.cpp
  crypto/crypto.cpp
  crypto/encrypted_frame.cpp
  crypto/types.cpp
  dht/context.cpp
  dht/dht.cpp
  dht/explorenetworkjob.cpp
  dht/localtaglookup.cpp
  dht/localrouterlookup.cpp
  dht/localserviceaddresslookup.cpp
  dht/message.cpp
  dht/messages/findintro.cpp
  dht/messages/findrouter.cpp
  dht/messages/gotintro.cpp
  dht/messages/gotrouter.cpp
  dht/messages/pubintro.cpp
  dht/publishservicejob.cpp
  dht/recursiverouterlookup.cpp
  dht/serviceaddresslookup.cpp
  dht/taglookup.cpp
  exit/context.cpp
  exit/endpoint.cpp
  exit/exit_messages.cpp
  exit/policy.cpp
  exit/session.cpp
  handlers/exit.cpp
  handlers/tun.cpp
  hook/shell.cpp
  iwp/iwp.cpp
  iwp/linklayer.cpp
  iwp/message_buffer.cpp
  iwp/session.cpp
  link/factory.cpp
  link/link_manager.cpp
  link/server.cpp
  messages/dht_immediate.cpp
  messages/link_intro.cpp
  messages/link_message_parser.cpp
  messages/relay.cpp
  messages/relay_commit.cpp
  messages/relay_status.cpp
  net/address_info.cpp
  net/exit_info.cpp
  nodedb.cpp
  path/ihophandler.cpp
  path/path_context.cpp
  path/path.cpp
  path/pathbuilder.cpp
  path/pathset.cpp
  path/transit_hop.cpp
  pow.cpp
  profiling.cpp
  router/outbound_message_handler.cpp
  router/outbound_session_maker.cpp
  router/rc_lookup_handler.cpp
  router/rc_gossiper.cpp
  router/router.cpp
  router_contact.cpp
  router_id.cpp
  router_version.cpp
  routing/dht_message.cpp
  routing/message_parser.cpp
  routing/path_confirm_message.cpp
  routing/path_latency_message.cpp
  routing/path_transfer_message.cpp
  routing/transfer_traffic_message.cpp
  rpc/rpc.cpp
  service/address.cpp
  service/async_key_exchange.cpp
  service/config.cpp
  service/context.cpp
  service/endpoint_state.cpp
  service/endpoint_util.cpp
  service/endpoint.cpp
  service/hidden_service_address_lookup.cpp
  service/identity.cpp
  service/info.cpp
  service/intro_set.cpp
  service/intro.cpp
  service/lookup.cpp
  service/outbound_context.cpp
  service/protocol.cpp
  service/router_lookup_job.cpp
  service/sendcontext.cpp
  service/session.cpp
  service/tag_lookup_job.cpp
  service/tag.cpp
)
if(TRACY_ROOT)
  set(LIB_SRC ${LIB_SRC} ${TRACY_ROOT}/TracyClient.cpp)
endif()

if(TESTNET)
  set(LIB_SRC ${LIB_SRC} testnet.c)
endif()

add_library(${STATIC_LIB} STATIC ${LIB_SRC})
target_include_directories(${STATIC_LIB} PUBLIC ${CURL_INCLUDE_DIRS})
target_link_libraries(${STATIC_LIB} PUBLIC cxxopts ${ABYSS_LIB} ${PLATFORM_LIB} ${UTIL_LIB} ${CRYPTOGRAPHY_LIB})

if(${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")
  target_include_directories(${PLATFORM_LIB} SYSTEM PUBLIC /usr/local/include)
  target_include_directories(${STATIC_LIB} SYSTEM PUBLIC /usr/local/include)
endif()

if(WITH_SHARED)
  add_library(${SHARED_LIB} SHARED ${LIB_SRC})
  set(LIBS ${LIBS} Threads::Threads)
  target_link_libraries(${SHARED_LIB} PUBLIC ${ABYSS_LIB} ${CRYPTOGRAPHY_LIB} ${UTIL_LIB} ${PLATFORM_LIB} ${LIBS})
  if (WIN32)
    target_link_libraries(${SHARED_LIB} PUBLIC ws2_32 iphlpapi)
  else()
    install(TARGETS ${SHARED_LIB} LIBRARY DESTINATION lib)
  endif()
  add_log_tag(${SHARED_LIB})
endif()

if (WARNINGS_AS_ERRORS)
  set(WARN_FLAGS -Wall -Wextra -Werror)
  target_compile_options(${UTIL_LIB} PUBLIC ${WARN_FLAGS})
  target_compile_options(${PLATFORM_LIB} PUBLIC ${WARN_FLAGS})
  target_compile_options(${STATIC_LIB} PUBLIC ${WARN_FLAGS})
endif()
add_log_tag(${UTIL_LIB})
add_log_tag(${PLATFORM_LIB})
add_log_tag(${STATIC_LIB})
