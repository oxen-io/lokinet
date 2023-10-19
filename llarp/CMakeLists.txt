include(Version)

add_library(lokinet-cryptography
  STATIC
  crypto/crypto.cpp
  crypto/encrypted_frame.cpp
  crypto/types.cpp
)

# files only included by lokinet-core
add_library(lokinet-core-internal
  STATIC
  router/rc_gossiper.cpp
  service/info.cpp
  service/lns_tracker.cpp
  service/session.cpp
)
  
# Functional objects use by lokinet-core and other libraries
# needed by vpn/ router/ rpc/ handlers/ net/ link/
add_library(lokinet-core-utils
  STATIC
  endpoint_base.cpp
  exit/context.cpp
  exit/endpoint.cpp # handlers/exit.hpp 
  exit/policy.cpp   # net/
  service/auth.cpp  # config/
  service/context.cpp
  service/identity.cpp
  service/intro.cpp # path
  service/name.cpp
  service/tag.cpp
)

add_library(lokinet-core
  STATIC
  context.cpp
  consensus/reachability_testing.cpp
  exit/session.cpp
  handlers/exit.cpp # link/ exit/
  handlers/tun.cpp
  link/link_manager.cpp
  router/router.cpp
  router/route_poker.cpp
  service/async_key_exchange.cpp
  service/endpoint_util.cpp
  service/endpoint.cpp
  service/endpoint_state.cpp
  service/name.cpp
  service/outbound_context.cpp
  service/protocol.cpp
  vpn/egres_packet_router.cpp
)
  
# Files only included by lokinet-rpc
# add_library(lokinet-rpc-internal
#   STATIC
# )
  
  add_library(lokinet-rpc
  STATIC
  rpc/json_binary_proxy.cpp
  rpc/json_conversions.cpp
  rpc/lokid_rpc_client.cpp
  rpc/rpc_request_parser.cpp
  rpc/rpc_server.cpp
  rpc/endpoint_rpc.cpp
)
  
# Files only included by lokinet-wire
add_library(lokinet-wire-internal
  STATIC
  link/connection.cpp
)
  
add_library(lokinet-wire
  STATIC
  link/contacts.cpp
  link/link_manager.cpp
  # link/tunnel.cpp
)

add_library(lokinet-utils-internal
  STATIC
  util/thread/queue_manager.cpp  
)

# config, crypto, timeplace, nodedb, bootstrap.cpp, net, router
add_library(lokinet-utils
  STATIC
  ${CMAKE_CURRENT_BINARY_DIR}/constants/version.cpp
  util/bencode.cpp
  util/buffer.cpp
  util/file.cpp
  util/mem.cpp
  util/str.cpp
  util/thread/threading.cpp
  util/time.cpp
  util/logging/buffer.cpp
)
  
add_dependencies(lokinet-utils genversion)

# Addressing and event loop files used by lokinet-core and other libraries
# needed by rpc/ link/ service/ config/ path/ dht/
add_library(lokinet-time-place
  STATIC
  pow.cpp # only intro_set
  router_contact.cpp
  router_id.cpp
  router_version.cpp  # to be deleted shortly
  ev/ev.cpp
  ev/libuv.cpp
  net/exit_info.cpp   # only router_contact
  net/ip.cpp
  net/ip_address.cpp
  net/ip_packet.cpp
  net/ip_range.cpp
  net/net_int.cpp
  net/sock_addr.cpp
  service/address.cpp
  service/convotag.cpp
  service/intro_set.cpp
)

# lokinet-platform holds all platform specific code
add_library(lokinet-platform
  STATIC
  router/rc_lookup_handler.cpp
  net/interface_info.cpp
  vpn/packet_router.cpp
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
add_library(lokinet-dns-internal
  STATIC
  dns/name.cpp      # srv_data, question, rr
  dns/platform.cpp
  dns/question.cpp  # message
  dns/rr.cpp
  dns/serialize.cpp
)

add_library(lokinet-dns
  STATIC
  dns/message.cpp   # dns/server
  dns/server.cpp    # handlers/exit+tun and service/endpoint
  dns/srv_data.cpp  
)

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
  bootstrap.cpp   # config, router.hpp
  nodedb.cpp
  profiling.cpp # path, router, service::endpoint
  net/traffic_policy.cpp  # config, intro_set
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
  config/key_manager.cpp
)

# All path objects; link directly to lokinet-core
add_library(lokinet-path
  STATIC
  path/abstracthophandler.cpp
  path/path_context.cpp
  path/path.cpp
  path/pathbuilder.cpp
  path/pathset.cpp
  path/transit_hop.cpp
  messages/relay.cpp
)

add_library(lokinet-amalgum INTERFACE)

# interface library for setting common includes, linkage and flags.
add_library(lokinet-base INTERFACE)
target_include_directories(lokinet-base
  INTERFACE ${PROJECT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/include
)
target_link_libraries(lokinet-base INTERFACE quic)

# Link libraries to their internals
target_link_libraries(lokinet-core-internal PUBLIC lokinet-utils lokinet-core-utils)
target_link_libraries(lokinet-core PRIVATE lokinet-core-internal)
# target_link_libraries(lokinet-rpc PRIVATE lokinet-rpc-internal)
target_link_libraries(lokinet-wire PRIVATE lokinet-wire-internal)
# target_link_libraries(lokinet-time-place PRIVATE lokinet-time-place-internal)
target_link_libraries(lokinet-dns PRIVATE lokinet-dns-internal)
target_link_libraries(lokinet-utils PRIVATE lokinet-utils-internal)

# Link lokinet-dns to alternate libraries
target_link_libraries(lokinet-dns PUBLIC lokinet-dns-platform)
if (TARGET lokinet-dns-systemd)
  target_link_libraries(lokinet-dns PUBLIC lokinet-dns-systemd)
endif()

# Link relevant to lokinet-base
target_link_libraries(lokinet-core PUBLIC lokinet-base)
target_link_libraries(lokinet-rpc PUBLIC lokinet-base)
target_link_libraries(lokinet-nodedb PUBLIC lokinet-base)
target_link_libraries(lokinet-wire PUBLIC lokinet-base)
target_link_libraries(lokinet-time-place PUBLIC lokinet-base)
target_link_libraries(lokinet-dns PUBLIC lokinet-base)
target_link_libraries(lokinet-platform PUBLIC lokinet-base)
target_link_libraries(lokinet-utils PUBLIC lokinet-base)
target_link_libraries(lokinet-libntrup PUBLIC lokinet-base)

# Link relevant to amalgum
target_link_libraries(lokinet-amalgum 
  INTERFACE
  lokinet-core
  lokinet-rpc
  lokinet-nodedb
  lokinet-wire
  lokinet-time-place
  lokinet-dns
  lokinet-platform
  lokinet-utils
  lokinet-cryptography
  lokinet-path
)
#or just do this??
# target_link_libraries(lokinet-amalgum INTERFACE lokinet-base)
target_link_libraries(lokinet-wire-internal PUBLIC lokinet-time-place)
target_link_libraries(lokinet-dns-internal PUBLIC lokinet-utils lokinet-config)
target_link_libraries(lokinet-dns PUBLIC lokinet-cryptography)
target_link_libraries(lokinet-nodedb PUBLIC lokinet-cryptography)
target_link_libraries(lokinet-platform PUBLIC lokinet-cryptography)
target_link_libraries(lokinet-rpc PUBLIC lokinet-wire)
target_link_libraries(lokinet-time-place PUBLIC lokinet-utils lokinet-cryptography)
target_link_libraries(lokinet-wire PUBLIC lokinet-cryptography)
target_link_libraries(lokinet-config PUBLIC lokinet-cryptography)

target_link_libraries(lokinet-path
  PUBLIC
  lokinet-time-place
)

target_link_libraries(lokinet-core-utils 
  PUBLIC 
  lokinet-core 
  lokinet-wire 
  lokinet-rpc 
  lokinet-platform
)

target_link_libraries(lokinet-cryptography
  PUBLIC
  lokinet-libcrypt
  lokinet-libntrup
)

target_link_libraries(lokinet-utils
  PUBLIC
  lokinet-cryptography
)

# cross linkage
target_link_libraries(lokinet-core 
  PUBLIC 
  lokinet-rpc 
  lokinet-wire 
  lokinet-nodedb 
  lokinet-config
  lokinet-cryptography
)


# per component external deps
# target_link_libraries(oxenmq::oxenmq
#   PUBLIC
#   lokinet-time-place
#   lokinet-core
#   lokinet-config
#   lokinet-platform
#   lokinet-dns
#   lokinet-rpc
# )

target_link_libraries(lokinet-rpc PUBLIC oxenmq::oxenmq oxenc::oxenc)
target_link_libraries(lokinet-core PUBLIC oxenmq::oxenmq CLI11 uvw)
target_link_libraries(lokinet-config PUBLIC oxenmq::oxenmq)
target_link_libraries(lokinet-nodedb PUBLIC oxenmq::oxenmq uvw)
target_link_libraries(lokinet-path PUBLIC oxenmq::oxenmq)
target_link_libraries(lokinet-time-place PUBLIC uvw oxenc::oxenc)
target_link_libraries(lokinet-platform PUBLIC oxenmq::oxenmq Threads::Threads base_libs uvw)
target_link_libraries(lokinet-cryptography PUBLIC oxenc::oxenc sodium)
target_link_libraries(lokinet-dns PUBLIC libunbound uvw)
target_link_libraries(lokinet-utils PUBLIC nlohmann_json::nlohmann_json oxenc::oxenc)
target_link_libraries(lokinet-wire PUBLIC oxenmq::oxenmq quic uvw)

# if(WITH_EMBEDDED_LOKINET)
#   include(GNUInstallDirs)
#   add_library(lokinet-shared SHARED lokinet_shared.cpp)
#   target_link_libraries(lokinet-shared PUBLIC lokinet-amalgum)
#   if(WIN32)
#     set(CMAKE_SHARED_LIBRARY_PREFIX_CXX "")
#   endif()
#   set_target_properties(lokinet-shared PROPERTIES OUTPUT_NAME lokinet)
#   if(WIN32)
#     target_link_libraries(lokinet-shared PUBLIC ws2_32 iphlpapi -fstack-protector)
#     install(TARGETS lokinet-shared DESTINATION bin COMPONENT liblokinet)
#   elseif(NOT APPLE)
#     install(TARGETS lokinet-shared LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT liblokinet)
#   endif()
# endif()

# file(GLOB_RECURSE docs_SRC */*.hpp *.hpp)
# set(DOCS_SRC ${docs_SRC} PARENT_SCOPE)
