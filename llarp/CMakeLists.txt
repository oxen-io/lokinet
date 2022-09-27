include(Version)

add_library(lokinet-util
  STATIC
  ${CMAKE_CURRENT_BINARY_DIR}/constants/version.cpp
  util/bencode.cpp
  util/buffer.cpp
  util/file.cpp
  util/json.cpp
  util/logging/buffer.cpp
  util/easter_eggs.cpp
  util/mem.cpp
  util/str.cpp
  util/thread/queue_manager.cpp
  util/thread/threading.cpp
  util/time.cpp)


add_dependencies(lokinet-util genversion)

target_include_directories(lokinet-util PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR})

target_link_libraries(lokinet-util PUBLIC
  lokinet-cryptography
  nlohmann_json::nlohmann_json
  filesystem
  oxenc::oxenc
  oxen::logging
)

add_library(lokinet-platform
  STATIC
  # for networking
  ev/ev.cpp
  ev/libuv.cpp
  net/ip.cpp
  net/ip_address.cpp
  net/ip_packet.cpp
  net/ip_range.cpp
  net/net_int.cpp
  net/sock_addr.cpp
  vpn/packet_router.cpp
  vpn/egres_packet_router.cpp
  vpn/platform.cpp
)

target_link_libraries(lokinet-platform PUBLIC lokinet-cryptography lokinet-util Threads::Threads base_libs uvw)
target_link_libraries(lokinet-platform PRIVATE oxenmq::oxenmq)

if (ANDROID)
  target_sources(lokinet-platform PRIVATE android/ifaddrs.c)
endif()

if(CMAKE_SYSTEM_NAME MATCHES "Linux")
  target_sources(lokinet-platform PRIVATE linux/dbus.cpp)
endif()

if (WIN32)
  target_sources(lokinet-platform PRIVATE
    net/win32.cpp
    vpn/win32.cpp
    win32/exec.cpp)
  add_library(lokinet-win32 STATIC
    win32/dll.cpp
    win32/exception.cpp)
  add_library(lokinet-wintun STATIC
    win32/wintun.cpp)
  add_library(lokinet-windivert STATIC
    win32/windivert.cpp)
    
  # wintun and windivert are privated linked by lokinet-platform
  # this is so their details do not leak out to deps of lokinet-platform
  # wintun and windivert still need things from lokinet-platform
  target_compile_options(lokinet-wintun PUBLIC -I${CMAKE_BINARY_DIR}/wintun/include/)
  target_compile_options(lokinet-windivert PUBLIC -I${CMAKE_BINARY_DIR}/WinDivert-${WINDIVERT_VERSION}/include/)
  target_include_directories(lokinet-windivert PUBLIC ${PROJECT_SOURCE_DIR})
  target_link_libraries(lokinet-wintun PUBLIC lokinet-platform lokinet-util lokinet-config)
  target_link_libraries(lokinet-win32 PUBLIC lokinet-util)
  target_link_libraries(lokinet-windivert PUBLIC oxen-logging)
  target_link_libraries(lokinet-windivert PRIVATE lokinet-win32)
  target_link_libraries(lokinet-platform PRIVATE lokinet-win32 lokinet-wintun lokinet-windivert)
else()
  target_sources(lokinet-platform PRIVATE
    net/posix.cpp)
endif()


if(CMAKE_SYSTEM_NAME MATCHES "FreeBSD")
  target_include_directories(lokinet-platform SYSTEM PUBLIC /usr/local/include)
endif()

add_library(lokinet-dns
  STATIC
  dns/message.cpp
  dns/name.cpp
  dns/platform.cpp
  dns/question.cpp
  dns/rr.cpp
  dns/serialize.cpp
  dns/server.cpp
  dns/srv_data.cpp)

if(WITH_SYSTEMD)
  target_sources(lokinet-dns PRIVATE dns/nm_platform.cpp dns/sd_platform.cpp)
endif()

target_link_libraries(lokinet-dns PUBLIC lokinet-platform uvw)
target_link_libraries(lokinet-dns PRIVATE libunbound lokinet-config)

add_library(lokinet-config
  STATIC
  config/config.cpp
  config/definition.cpp
  config/ini.cpp
  config/key_manager.cpp)

target_link_libraries(lokinet-config PUBLIC lokinet-dns lokinet-platform oxenmq::oxenmq)

add_library(lokinet-amalgum
  STATIC
  consensus/reachability_testing.cpp

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
  dht/messages/findname.cpp
  dht/messages/gotname.cpp
  dht/publishservicejob.cpp
  dht/recursiverouterlookup.cpp
  dht/serviceaddresslookup.cpp
  dht/taglookup.cpp

  endpoint_base.cpp

  exit/context.cpp
  exit/endpoint.cpp
  exit/exit_messages.cpp
  exit/policy.cpp
  exit/session.cpp
  handlers/exit.cpp
  handlers/tun.cpp
  iwp/iwp.cpp
  iwp/linklayer.cpp
  iwp/message_buffer.cpp
  iwp/session.cpp
  link/link_manager.cpp
  link/session.cpp
  link/server.cpp
  messages/dht_immediate.cpp
  messages/link_intro.cpp
  messages/link_message_parser.cpp
  messages/relay.cpp
  messages/relay_commit.cpp
  messages/relay_status.cpp
  net/address_info.cpp
  net/exit_info.cpp
  net/traffic_policy.cpp
  nodedb.cpp
  path/ihophandler.cpp
  path/path_context.cpp
  path/path.cpp
  path/pathbuilder.cpp
  path/pathset.cpp
  path/transit_hop.cpp
  peerstats/peer_db.cpp
  peerstats/types.cpp
  pow.cpp
  profiling.cpp
  
  quic/address.cpp
  quic/client.cpp
  quic/connection.cpp
  quic/endpoint.cpp
  quic/null_crypto.cpp
  quic/server.cpp
  quic/stream.cpp
  quic/tunnel.cpp

  router_contact.cpp
  router_id.cpp
  router_version.cpp
  service/name.cpp
  router/outbound_message_handler.cpp
  router/outbound_session_maker.cpp
  router/rc_lookup_handler.cpp
  router/rc_gossiper.cpp
  router/router.cpp
  router/route_poker.cpp

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
  service/convotag.cpp
  service/context.cpp
  service/endpoint_state.cpp
  service/endpoint_util.cpp
  service/endpoint.cpp
  service/hidden_service_address_lookup.cpp
  service/identity.cpp
  service/info.cpp
  service/intro_set.cpp
  service/intro.cpp
  service/lns_tracker.cpp
  service/lookup.cpp
  service/name.cpp
  service/outbound_context.cpp
  service/protocol.cpp
  service/router_lookup_job.cpp
  service/sendcontext.cpp
  service/session.cpp
  service/tag.cpp
)

if(BOOTSTRAP_SYSTEM_PATH)
  message(STATUS "Building with fallback boostrap path \"${BOOTSTRAP_SYSTEM_PATH}\"")
  target_compile_definitions(lokinet-amalgum PRIVATE "BOOTSTRAP_FALLBACK=\"${BOOTSTRAP_SYSTEM_PATH}\"")
endif()

if(WITH_PEERSTATS_BACKEND)
  target_compile_definitions(lokinet-amalgum PRIVATE -DLOKINET_PEERSTATS_BACKEND)
  target_link_libraries(lokinet-amalgum PUBLIC sqlite_orm)
endif()

if(WITH_HIVE)
  target_sources(lokinet-amalgum PRIVATE
    tooling/router_hive.cpp
    tooling/hive_router.cpp
    tooling/hive_context.cpp
  )
endif()

# TODO: make libunbound hidden behind a feature flag like sqlite for embedded lokinet
target_link_libraries(lokinet-amalgum PRIVATE libunbound)

target_link_libraries(lokinet-amalgum PUBLIC
    cxxopts
    oxenc::oxenc
    lokinet-platform
    lokinet-config
    lokinet-dns
    lokinet-util
    lokinet-cryptography
    ngtcp2_static
    oxenmq::oxenmq)

enable_lto(lokinet-util lokinet-platform lokinet-dns lokinet-config lokinet-amalgum)

pkg_check_modules(CRYPT libcrypt IMPORTED_TARGET)
if(CRYPT_FOUND AND NOT CMAKE_CROSSCOMPILING)
  add_definitions(-DHAVE_CRYPT)
  add_library(libcrypt INTERFACE)
  target_link_libraries(libcrypt INTERFACE PkgConfig::CRYPT)
  target_link_libraries(lokinet-amalgum PRIVATE libcrypt)
  message(STATUS "using libcrypt ${CRYPT_VERSION}")
endif()


if(BUILD_LIBLOKINET)
  include(GNUInstallDirs)
  add_library(lokinet-shared SHARED lokinet_shared.cpp)
  target_link_libraries(lokinet-shared PUBLIC lokinet-amalgum)
  if(WIN32)
    set(CMAKE_SHARED_LIBRARY_PREFIX_CXX "")
  endif()
  set_target_properties(lokinet-shared PROPERTIES OUTPUT_NAME lokinet)
  if(WIN32)
    target_link_libraries(lokinet-shared PUBLIC ws2_32 iphlpapi -fstack-protector)
    install(TARGETS lokinet-shared DESTINATION bin COMPONENT liblokinet)
  elseif(NOT APPLE)
    install(TARGETS lokinet-shared LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT liblokinet)
  endif()
endif()

if(APPLE)
  add_subdirectory(apple)
endif()

file(GLOB_RECURSE docs_SRC */*.hpp *.hpp)

set(DOCS_SRC ${docs_SRC} PARENT_SCOPE)
