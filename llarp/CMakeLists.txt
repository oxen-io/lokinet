include(Version)

target_sources(lokinet-cryptography PRIVATE
  crypto/crypto_libsodium.cpp
  crypto/crypto.cpp
  crypto/encrypted_frame.cpp
  crypto/types.cpp
)

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

# lokinet-platform holds all platform specific code
add_library(lokinet-platform
  STATIC
  # for networking
  ev/ev.cpp
  ev/libuv.cpp
  net/interface_info.cpp
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

if (ANDROID)
  target_sources(lokinet-platform PRIVATE android/ifaddrs.c util/nop_service_manager.cpp)
endif()

if(CMAKE_SYSTEM_NAME MATCHES "Linux")
  target_sources(lokinet-platform PRIVATE linux/dbus.cpp)
  if(WITH_SYSTEMD)
    target_sources(lokinet-platform PRIVATE linux/sd_service_manager.cpp)
  else()
    target_sources(lokinet-platform PRIVATE util/nop_service_manager.cpp)
  endif()
endif()

if (WIN32)
  target_sources(lokinet-platform PRIVATE
    net/win32.cpp
    vpn/win32.cpp
    win32/service_manager.cpp
    win32/exec.cpp
    win32/dll.cpp
    win32/exception.cpp
    win32/wintun.cpp
    win32/windivert.cpp)
  target_include_directories(lokinet-platform PRIVATE ${CMAKE_BINARY_DIR}/wintun/include/ ${CMAKE_BINARY_DIR}/WinDivert-${WINDIVERT_VERSION}/include/)
else()
  target_sources(lokinet-platform PRIVATE
    net/posix.cpp)
endif()

if(APPLE)
  add_subdirectory(apple)
  target_sources(lokinet-platform PRIVATE util/nop_service_manager.cpp)
endif()

# lokinet-dns is the dns parsing and hooking library that we use to
# parse modify and reconstitute dns wire proto, dns queries and RR
# should have no concept of dns caching, this is left as an implementation
# detail of dns resolvers (LATER: make separate lib for dns resolvers)
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

# platform specific bits and bobs for setting dns
add_library(lokinet-dns-platform INTERFACE)
if(WITH_SYSTEMD)
  add_library(lokinet-dns-systemd STATIC dns/nm_platform.cpp dns/sd_platform.cpp)
  target_link_libraries(lokinet-dns-platform INTERFACE lokinet-dns-systemd)
endif()

# lokinet-nodedb holds all types and logic for storing parsing and constructing
# nodedb data published to the network and versions of it stored locally
add_library(lokinet-nodedb
  STATIC
  bootstrap.cpp
  net/address_info.cpp
  net/exit_info.cpp
  net/traffic_policy.cpp
  nodedb.cpp
  pow.cpp
  profiling.cpp
  router_contact.cpp
  router_id.cpp
  router_version.cpp
)

set(BOOTSTRAP_FALLBACKS)
foreach(bs IN ITEMS MAINNET TESTNET)
  if(BOOTSTRAP_FALLBACK_${bs})
    message(STATUS "Building with ${bs} fallback boostrap path \"${BOOTSTRAP_FALLBACK_${bs}}\"")
    file(READ "${BOOTSTRAP_FALLBACK_${bs}}" bs_data HEX)
    if(bs STREQUAL TESTNET)
      set(network "gamma")
    elseif(bs STREQUAL MAINNET)
      set(network "lokinet")
    else()
      string(TOLOWER "${bs}" network)
    endif()
    string(REGEX REPLACE "([0-9a-f][0-9a-f])" "\\\\x\\1" bs_data "${bs_data}")
    set(BOOTSTRAP_FALLBACKS "${BOOTSTRAP_FALLBACKS}{\"${network}\"s, \"${bs_data}\"sv},\n")
  endif()
endforeach()
configure_file("bootstrap-fallbacks.cpp.in" "${CMAKE_CURRENT_BINARY_DIR}/bootstrap-fallbacks.cpp" @ONLY)
target_sources(lokinet-nodedb PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/bootstrap-fallbacks.cpp")


# lokinet-config is for all configuration types and parsers
add_library(lokinet-config
  STATIC
  config/config.cpp
  config/definition.cpp
  config/ini.cpp
  config/key_manager.cpp)

# lokinet-consensus is for deriving and tracking network consensus state for both service nodes and clients
add_library(lokinet-consensus
  STATIC
  consensus/reachability_testing.cpp
)

# lokinet-dht holds all logic related to interacting with and participating in the DHT hashring
add_library(lokinet-dht
  STATIC
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
)

# lokinet-layer-flow is the flow layer which sits atop the routing layer which manages
# flows between lokinet snapp endpoints be they .loki or .snode
add_library(lokinet-layer-flow
  STATIC
  layers/flow/stub.cpp # todo: remove me
)


# lokinet-layer-onion is the "dumb" onion routing layer with builds manages and does i/o
# with onion paths. onion paths anonymize routing layer pdu.
add_library(lokinet-layer-onion
  STATIC
  path/ihophandler.cpp
  path/path_context.cpp
  path/path.cpp
  path/pathbuilder.cpp
  path/pathset.cpp
  path/transit_hop.cpp
  messages/relay.cpp
  messages/relay_commit.cpp
  messages/relay_status.cpp
)

# lokinet-layer-wire is a layer 1 analog which splits up
# layer 2 frames into layer 1 symbols which in the case of iwp are encrypted udp/ip packets
add_library(lokinet-layer-wire
  STATIC
  iwp/iwp.cpp
  iwp/linklayer.cpp
  iwp/message_buffer.cpp
  iwp/session.cpp
)

# lokinet-layer-link is for our layer 2 analog which splits up layer 2 frames into
# a series of layer 1 symbols which are then transmitted between lokinet instances
add_library(lokinet-layer-link
  STATIC
  link/link_manager.cpp
  link/session.cpp
  link/server.cpp
  messages/dht_immediate.cpp
  messages/link_intro.cpp
  messages/link_message_parser.cpp
)

# lokinet-plainquic is for holding the tunneled plainquic code, not quic wire protocol code
add_library(lokinet-plainquic
  STATIC
  quic/address.cpp
  quic/client.cpp
  quic/connection.cpp
  quic/endpoint.cpp
  quic/null_crypto.cpp
  quic/server.cpp
  quic/stream.cpp
  quic/tunnel.cpp
)

# lokinet-context holds the contextualized god objects for a lokinet instance
# it is what any main function would link to in practice but it is hidden behind an interface library (lokinet-amalgum)
add_library(lokinet-context
  STATIC
  context.cpp
  link/link_manager.cpp
  router/outbound_message_handler.cpp
  router/outbound_session_maker.cpp
  router/rc_lookup_handler.cpp
  router/rc_gossiper.cpp
  router/router.cpp
  router/route_poker.cpp
)

# lokinet-rpc holds all rpc related compilation units
add_library(lokinet-rpc
  STATIC
  rpc/json_binary_proxy.cpp
  rpc/json_conversions.cpp
  rpc/lokid_rpc_client.cpp
  rpc/rpc_request_parser.cpp
  rpc/rpc_server.cpp
  rpc/endpoint_rpc.cpp
)

# optional peer stats library
add_library(lokinet-peerstats
  STATIC
  peerstats/peer_db.cpp
  peerstats/types.cpp
)

# lokinet-layer-routing holds logic related to the routing layer
# routing layer is anonymized over the onion layer
add_library(lokinet-layer-routing
  STATIC
  routing/dht_message.cpp
  routing/message_parser.cpp
  routing/path_confirm_message.cpp
  routing/path_latency_message.cpp
  routing/path_transfer_message.cpp
  routing/transfer_traffic_message.cpp
)

# kitchen sink to be removed after refactor
add_library(lokinet-service-deprecated-kitchensink
  STATIC
  endpoint_base.cpp
  exit/context.cpp
  exit/endpoint.cpp
  exit/exit_messages.cpp
  exit/policy.cpp
  exit/session.cpp
  handlers/exit.cpp
  handlers/tun.cpp
  service/name.cpp
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

add_library(lokinet-layer-platform
  STATIC
  layers/platform/stub.cpp # todo: remove me
)


# interal tooling for pybind
add_library(lokinet-tooling INTERFACE)
if(WITH_HIVE)
  add_library(lokinet-hive-tooling
    STATIC
    tooling/router_hive.cpp
    tooling/hive_router.cpp
    tooling/hive_context.cpp
  )
  target_link_libraries(lokinet-tooling INTERFACE lokinet-hive-tooling)
endif()


# interface library for setting commone includes, linkage and flags.
add_library(lokinet-base INTERFACE)
target_include_directories(lokinet-base
  INTERFACE ${PROJECT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/include
)
target_link_libraries(lokinet-base INTERFACE oxen::logging lokinet-cryptography)

if(WITH_PEERSTATS)
  target_compile_definitions(lokinet-base INTERFACE -DLOKINET_PEERSTATS_BACKEND)
  target_link_libraries(lokinet-base INTERFACE sqlite_orm)
endif()

# interface libraries for internal linkage
add_library(lokinet-layers INTERFACE)
add_library(lokinet-amalgum INTERFACE)


# helper function to link a library to lokinet-base, enable lto, add to lokinet-amalgum and then link to other libs
function(lokinet_link_lib libname)
  message(DEBUG "created target: ${libname}")
  enable_lto(${libname})
  target_link_libraries(${libname} PUBLIC lokinet-base ${ARGN})
  target_link_libraries(lokinet-amalgum INTERFACE ${libname})
endfunction()

# internal public linkages of components
lokinet_link_lib(lokinet-util)
lokinet_link_lib(lokinet-cryptography lokinet-libcrypt lokinet-util)
lokinet_link_lib(lokinet-peerstats lokinet-context)
lokinet_link_lib(lokinet-consensus lokinet-context)
lokinet_link_lib(lokinet-layer-link lokinet-peerstats)

if(TARGET lokinet-hive-tooling)
  lokinet_link_lib(lokinet-hive-tooling lokinet-context)
endif()

if(TARGET lokinet-dns-systemd)
  lokinet_link_lib(lokinet-dns-systemd
    lokinet-dns
    lokinet-platform
  )
endif()

lokinet_link_lib(lokinet-platform lokinet-util)

lokinet_link_lib(lokinet-config
  lokinet-util
  lokinet-nodedb
  lokinet-dns
  lokinet-platform
)

lokinet_link_lib(lokinet-context
  lokinet-config
  lokinet-platform
  lokinet-peerstats
  lokinet-layers
  lokinet-consensus
  lokinet-rpc
)

lokinet_link_lib(lokinet-dht
  lokinet-util
  lokinet-nodedb
)

lokinet_link_lib(lokinet-plainquic
  lokinet-platform
  lokinet-config
)

lokinet_link_lib(lokinet-dns
  lokinet-platform
  lokinet-dns-platform
  lokinet-config
)

lokinet_link_lib(lokinet-nodedb
  lokinet-util
  lokinet-platform
)

lokinet_link_lib(lokinet-util
  lokinet-nodedb
  lokinet-platform
)

lokinet_link_lib(lokinet-rpc
  lokinet-context
  lokinet-peerstats
  lokinet-util
)

# inter lokinet-layer public/private linkage.
# when linking each layer, we consider the layer directly below private linkage and the layer above public linkage.
# this lets us hide functionality of layers below us when depended on by another component.
#
# from highest to lowest layer, the above layers are stacked as follows:
#
# platform (what lokinet snapps interact with, be it l3 os interaction or embedded lokinet)
# flow     (how we want to route and stripe over our onion routing)
# routing  (what we are onion routing)
# onion    (how the onion routing happens)
# link     (what we want to send over the wire and to where)
# wire     (what is actually sent over the wire)
#
function(link_lokinet_layers)
  set(lib ${ARGV0})
  if(${ARGC} GREATER 1)
    lokinet_link_lib(${ARGV1} ${lib})
    list(REMOVE_AT ARGV 1)
    target_link_libraries(${lib} PRIVATE ${ARGV1})
    # recursion :D 
    link_lokinet_layers(${ARGV})
  else()
    lokinet_link_lib(${lib})
  endif()
endfunction()

link_lokinet_layers(
  lokinet-layer-platform
  lokinet-layer-flow
  lokinet-layer-routing
  lokinet-layer-onion
  lokinet-layer-link
  lokinet-layer-wire
)

# set me to OFF to disable old codepath
set(use_old_impl ON)
if(use_old_impl)
  # flow layer deprecated-kitchensink (remove me after refactor)
  lokinet_link_lib(lokinet-service-deprecated-kitchensink
    lokinet-dns
    lokinet-nodedb
    lokinet-context
    lokinet-plainquic
    lokinet-layer-routing
    lokinet-layer-onion
    lokinet-dht
    lokinet-platform
    lokinet-rpc
  )
  target_link_libraries(lokinet-layers INTERFACE lokinet-service-deprecated-kitchensink)
endif()

target_link_libraries(lokinet-layers INTERFACE
  lokinet-layer-platform
  lokinet-layer-flow
  lokinet-layer-routing
  lokinet-layer-onion
  lokinet-layer-link
  lokinet-layer-wire
)


# per component external deps

target_link_libraries(lokinet-config PUBLIC oxenmq::oxenmq)
target_link_libraries(lokinet-platform PUBLIC oxenmq::oxenmq)
target_link_libraries(lokinet-dns PUBLIC libunbound)

target_link_libraries(lokinet-cryptography PUBLIC
  oxenc::oxenc
  sodium
)

target_link_libraries(lokinet-context PUBLIC
  CLI11
  oxenmq::oxenmq
  uvw
)

target_link_libraries(lokinet-platform PUBLIC
  Threads::Threads
  base_libs
  uvw
)

target_link_libraries(lokinet-util PUBLIC
  nlohmann_json::nlohmann_json
  filesystem
  oxenc::oxenc
)

target_link_libraries(lokinet-plainquic PUBLIC
  ngtcp2_static
  uvw
)

if(WITH_EMBEDDED_LOKINET)
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

if(WITH_EMBEDDED_LOKINET)
  add_library(lokinet-embedded-api STATIC lokinet_shared.cpp)
  target_link_libraries(lokinet-embedded-api PUBLIC liblokinet)
  add_log_tag(lokinet-embedded-api)
  combine_archives(lokinet-embedded
    lokinet-embedded-api
    lokinet-platform lokinet-util lokinet-cryptography sqlite3 ngtcp2_static
    liblokinet
    libuv
    sodium
    zlib
    OpenSSL::SSL OpenSSL::Crypto
    expat
    libunbound
    libzmq
    oxenmq)
  if(IOS)
    set(lib_folder lib-${ARCH})
  else()
    set(lib_folder lib)
  endif()
  install(TARGETS lokinet-embedded
    ARCHIVE DESTINATION ${lib_folder}
    EXCLUDE_FROM_ALL)
endif()

if(APPLE)
  add_subdirectory(apple)
  target_sources(lokinet-platform PRIVATE util/nop_service_manager.cpp)
endif()

file(GLOB_RECURSE docs_SRC */*.hpp *.hpp)
set(DOCS_SRC ${docs_SRC} PARENT_SCOPE)
