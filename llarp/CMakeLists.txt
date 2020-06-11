include(Version)

add_library(lokinet-util
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
  util/logging/logger_internal.cpp
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
  util/thread/threading.cpp
  util/time.cpp
)
add_dependencies(lokinet-util genversion)

target_include_directories(lokinet-util PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/include)

target_link_libraries(lokinet-util PUBLIC
  lokinet-cryptography
  nlohmann_json::nlohmann_json
  filesystem
  date::date
)

if(ANDROID)
  target_link_libraries(lokinet-util PUBLIC log)
endif()


add_library(lokinet-platform
# for networking
  ev/ev.cpp
  ev/pipe.cpp
  ev/ev_libuv.cpp

  net/ip.cpp
  net/ip_address.cpp
  net/ip_packet.cpp
  net/ip_range.cpp
  net/net.cpp
  net/net_int.cpp
  net/sock_addr.cpp
  $<TARGET_OBJECTS:tuntap>
)

target_link_libraries(lokinet-platform PUBLIC lokinet-cryptography lokinet-util Threads::Threads ${LIBS})


if (ANDROID)
  target_sources(lokinet-platform PRIVATE android/ifaddrs.c)
endif()

if(CMAKE_SYSTEM_NAME MATCHES "Linux")
  target_sources(lokinet-platform PRIVATE linux/netns.cpp)

  if(NON_PC_TARGET)
    add_import_library(rt)
    target_link_libraries(lokinet-platform PUBLIC rt)
  endif()
endif()

if (WIN32)
  target_sources(lokinet-platform PRIVATE
    ev/ev_libuv.cpp
    ev/ev_win32.cpp
    win32/win32_inet.c
    win32/win32_intrnl.c)

  target_link_libraries(lokinet-platform PUBLIC iphlpapi)
endif()

if(CMAKE_SYSTEM_NAME MATCHES "FreeBSD")
  target_include_directories(lokinet-platform SYSTEM PUBLIC /usr/local/include)
endif()

add_library(liblokinet
  config/config.cpp
  config/definition.cpp
  config/ini.cpp
  config/key_manager.cpp

  dns/message.cpp
  dns/name.cpp
  dns/question.cpp
  dns/rr.cpp
  dns/serialize.cpp
  dns/server.cpp
  dns/unbound_resolver.cpp

  consensus/table.cpp

  ev/vpnio.cpp

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
  rpc/lokid_rpc_client.cpp
  rpc/rpc_server.cpp
  rpc/endpoint_rpc.cpp
  service/address.cpp
  service/async_key_exchange.cpp
  service/auth.cpp
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

if(TARGET curl)
  # key_manager needs curl for now (to fetch service node keys from lokid)
  target_link_libraries(liblokinet PUBLIC curl)
  target_compile_definitions(liblokinet PRIVATE HAVE_CURL)
  if(TARGET libcurl_external)
    add_dependencies(liblokinet libcurl_external)
  endif()
endif()

set_target_properties(liblokinet PROPERTIES OUTPUT_NAME lokinet)

enable_lto(lokinet-util lokinet-platform liblokinet)
  
if(TRACY_ROOT)
  target_sources(liblokinet PRIVATE ${TRACY_ROOT}/TracyClient.cpp)
endif()

if(TESTNET)
  target_sources(liblokinet PRIVATE testnet.c)
endif()

if(WITH_HIVE)
  target_sources(liblokinet PRIVATE tooling/router_hive.cpp)
endif()

target_link_libraries(liblokinet PUBLIC cxxopts lokinet-platform lokinet-util lokinet-cryptography lokimq)
target_link_libraries(liblokinet PRIVATE libunbound)


if(BUILD_SHARED_LIBS)
  install(TARGETS lokinet-util lokinet-platform liblokinet LIBRARY DESTINATION lib)
  if(WIN32)
    target_link_libraries(liblokinet PUBLIC ws2_32 iphlpapi)
  endif()
endif()

foreach(lokinet_lib liblokinet lokinet-platform lokinet-util lokinet-cryptography)
  add_log_tag(${lokinet_lib})
endforeach()

file(GLOB_RECURSE docs_SRC */*.hpp *.hpp)
  
set(DOCS_SRC ${docs_SRC} PARENT_SCOPE)
