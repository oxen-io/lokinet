include(Version)

# Add an internal lokinet static library target, enables LTO (if enabled) on the target,
# and links it to the common lokinet-base interface.
# Invoke with the target/library name (e.g. "lokinet-foo") and list of source files, e.g.
#     lokinet_add_library(lokinet-foo foo/source1.cpp foo/source2.cpp)
function(lokinet_add_library libname)
    add_library(${libname} STATIC ${ARGN})
    target_link_libraries(${libname} PUBLIC lokinet-base PRIVATE lokinet-base-internal)
    enable_lto(${libname})
endfunction()

lokinet_add_library(lokinet-cryptography
  crypto/crypto.cpp
  crypto/types.cpp
  crypto/key_manager.cpp
)

# Functional objects use by lokinet-core and other libraries
# needed by vpn/ router/ rpc/ handlers/ net/ link/
lokinet_add_library(lokinet-core-utils
  auth/file_auth.cpp
  auth/rpc_auth.cpp
  auth/session_auth.cpp
  
  handlers/session.cpp

  messages/common.cpp
  messages/dht.cpp
  messages/fetch.cpp
  messages/path.cpp
  messages/session.cpp
  
)

if (NOT LOKINET_LIBRARY_ONLY)
  target_sources(lokinet-core-utils PRIVATE handlers/tun.cpp vpn/egres_packet_router.cpp)
endif()
  
lokinet_add_library(lokinet-core
  context.cpp
  
  consensus/reachability_testing.cpp
  
  link/link_manager.cpp
  
  router/router.cpp

  session/session.cpp
  session/map.cpp
)

if (NOT LOKINET_LIBRARY_ONLY)
  target_sources(lokinet-core PRIVATE router/route_poker.cpp)
endif()

lokinet_add_library(lokinet-rpc
  rpc/json_binary_proxy.cpp
  rpc/json_conversions.cpp
  rpc/rpc_client.cpp
  rpc/rpc_request_parser.cpp
  rpc/rpc_server.cpp
)

lokinet_add_library(lokinet-wire
  link/connection.cpp
  link/link_manager.cpp
)

# config, crypto, timeplace, nodedb, bootstrap.cpp, net, router
lokinet_add_library(lokinet-utils
  ${CMAKE_CURRENT_BINARY_DIR}/constants/version.cpp
  util/buffer.cpp
  util/file.cpp
  util/mem.cpp
  util/str.cpp
  util/thread/threading.cpp
  util/thread/queue_manager.cpp  
  util/time.cpp
)

add_dependencies(lokinet-utils genversion)

lokinet_add_library(lokinet-contact
  contact/client_contact.cpp
  contact/client_intro.cpp
  contact/contactdb.cpp
  contact/keys.cpp
  contact/relay_contact.cpp
  contact/router_id.cpp
  contact/sns.cpp
  contact/tag.cpp
)

# Definitely a better way to do this, but this works for now
if (LOKINET_LIBRARY_ONLY)
  target_sources(lokinet-contact PRIVATE dns/srv_data.cpp)
endif()

# Network interface files used for mediating ip traffic
lokinet_add_library(lokinet-ip
  net/ip_packet.cpp
  net/policy.cpp
  net/utils.cpp
)

# Addressing and event loop files used by lokinet-core and other libraries
# needed by rpc/ link/ config/ path/ dht/
lokinet_add_library(lokinet-addressing
  address/address.cpp
  address/ip_range.cpp
  address/utils.cpp

  ev/tcp.cpp
  ev/types.cpp

  router_version.cpp  # to be deleted shortly
)

if (NOT LOKINET_LIBRARY_ONLY)
  # lokinet-platform holds all platform specific code
  lokinet_add_library(lokinet-platform
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
  lokinet_add_library(lokinet-dns
    dns/message.cpp   # dns/server
    dns/name.cpp      # srv_data, question, rr
    dns/platform.cpp
    dns/question.cpp  # message
    dns/rr.cpp
    dns/serialize.cpp
    dns/server.cpp
    dns/srv_data.cpp
  )

  # platform specific bits and bobs for setting dns
  add_library(lokinet-dns-platform INTERFACE)
  if(WITH_SYSTEMD)
    lokinet_add_library(lokinet-dns-systemd dns/nm_platform.cpp dns/sd_platform.cpp)
    target_link_libraries(lokinet-dns-platform INTERFACE lokinet-dns-systemd)
  endif()
endif()

# lokinet-nodedb holds all types and logic for storing parsing and constructing
# nodedb data published to the network and versions of it stored locally
lokinet_add_library(lokinet-nodedb
  bootstrap.cpp   # config, router.hpp
  nodedb.cpp
  profiling.cpp # path, router, service::endpoint
)

set(BOOTSTRAP_FALLBACKS)
foreach(bs IN ITEMS MAINNET TESTNET)
  if(LOKINET_BOOTSTRAP_FALLBACK_${bs})
    message(STATUS "Building with ${bs} fallback bootstrap path \"${LOKINET_BOOTSTRAP_FALLBACK_${bs}}\"")
    file(READ "${LOKINET_BOOTSTRAP_FALLBACK_${bs}}" bs_data HEX)
    string(REGEX REPLACE "([0-9a-f][0-9a-f])" "\\\\x\\1" bs_data "${bs_data}")
    set(BOOTSTRAP_FALLBACKS "${BOOTSTRAP_FALLBACKS}        {NetID::${bs}, \"${bs_data}\"sv},\n")
  endif()
endforeach()
configure_file("bootstrap-fallbacks.cpp.in" "${CMAKE_CURRENT_BINARY_DIR}/bootstrap-fallbacks.cpp" @ONLY)
target_sources(lokinet-nodedb PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/bootstrap-fallbacks.cpp")

# lokinet-config is for all configuration types and parsers
lokinet_add_library(lokinet-config
  auth/auth.cpp
  config/config.cpp
  config/definition.cpp
  config/ini.cpp
)

# All path objects; link directly to lokinet-core
lokinet_add_library(lokinet-path
  path/path.cpp
  path/path_context.cpp
  path/path_handler.cpp
  path/transit_hop.cpp
)

# Link libraries to their internals
target_link_libraries(lokinet-wire PUBLIC lokinet-addressing)
target_link_libraries(lokinet-nodedb PUBLIC lokinet-addressing lokinet-cryptography)
target_link_libraries(lokinet-rpc PUBLIC lokinet-wire lokinet-contact lokinet-config)
target_link_libraries(lokinet-addressing PUBLIC lokinet-utils lokinet-cryptography lokinet-contact lokinet-ip)
target_link_libraries(lokinet-wire PUBLIC lokinet-cryptography)
target_link_libraries(lokinet-config PUBLIC lokinet-cryptography)

if (NOT LOKINET_LIBRARY_ONLY)
  target_link_libraries(lokinet-platform PUBLIC lokinet-cryptography lokinet-ip)
  target_link_libraries(lokinet-dns
    PUBLIC
    lokinet-addressing
    lokinet-cryptography
    lokinet-config
    lokinet-dns-platform
  )
endif()

target_link_libraries(lokinet-path
  PUBLIC
  lokinet-addressing
)

target_link_libraries(lokinet-core-utils 
  PUBLIC 
  lokinet-config
  lokinet-rpc 
  lokinet-wire 
)
if (NOT LOKINET_LIBRARY_ONLY)
  target_link_libraries(lokinet-core-utils
    PUBLIC
    lokinet-dns
    lokinet-platform
  )
endif()

target_link_libraries(lokinet-cryptography
  PUBLIC
  lokinet-libcrypt
)

target_link_libraries(lokinet-utils
  PUBLIC
  lokinet-cryptography
)

# cross linkage
target_link_libraries(lokinet-core 
  PUBLIC 
  lokinet-core-utils
  lokinet-cryptography
  lokinet-nodedb
  lokinet-path
  lokinet-rpc 
  lokinet-wire 
)


target_link_libraries(lokinet-base 
  INTERFACE 

  Threads::Threads

  oxenc::oxenc 
  oxen::logging 
  oxenmq::oxenmq 
  
  libunbound

  sodium
)


if(LOKINET_EMBEDDED)
  include(GNUInstallDirs)
  add_library(lokinet-embedded lokinet.cpp)
  target_link_libraries(lokinet-embedded PUBLIC lokinet-core)
  if(WIN32)
    set(CMAKE_SHARED_LIBRARY_PREFIX_CXX "")
  endif()
  set_target_properties(lokinet-embedded PROPERTIES OUTPUT_NAME lokinet)
  if(WIN32)
    target_link_libraries(lokinet-embedded PUBLIC ws2_32 iphlpapi -fstack-protector)
    install(TARGETS lokinet-embedded DESTINATION bin COMPONENT liblokinet)
  elseif(NOT APPLE)
    install(TARGETS lokinet-embedded LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT liblokinet)
  endif()

  add_library(lokinet::embedded ALIAS lokinet-embedded)
endif()

file(GLOB_RECURSE docs_SRC */*.hpp *.hpp)
set(DOCS_SRC ${docs_SRC} PARENT_SCOPE)
