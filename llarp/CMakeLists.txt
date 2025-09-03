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
  crypto/key_manager.cpp
  crypto/keys.cpp
  crypto/types.cpp
)

if(LOKINET_FULL)
  lokinet_add_library(lokinet-exit-auth
    auth/auth.cpp
    auth/file.cpp
    auth/rpc.cpp
    auth/session.cpp
  )
endif()

# Functional objects use by lokinet-core and other libraries
# needed by vpn/ router/ rpc/ handlers/ net/ link/
lokinet_add_library(lokinet-core-utils
  handlers/session.cpp

  messages/common.cpp
  messages/dht.cpp
  messages/fetch.cpp
  messages/path.cpp
  messages/session.cpp
)

if (LOKINET_FULL)
  target_sources(lokinet-core-utils PRIVATE handlers/tun.cpp vpn/egres_packet_router.cpp)
endif()

lokinet_add_library(lokinet-core
  context.cpp
  
  consensus/reachability_testing.cpp
  
  link/connection.cpp
  link/endpoint.cpp
  link/link_manager.cpp

  router/router.cpp

  session/session.cpp
)

if (LOKINET_FULL)
  target_sources(lokinet-core PRIVATE router/route_poker.cpp)
endif()

if (LOKINET_FULL)
  lokinet_add_library(lokinet-rpc
    rpc/json_binary_proxy.cpp
    rpc/json_conversions.cpp
    rpc/rpc_client.cpp
    rpc/rpc_request_parser.cpp
    rpc/rpc_server.cpp
  )
endif()

# config, crypto, timeplace, nodedb, bootstrap.cpp, net, router
lokinet_add_library(lokinet-utils
  ${CMAKE_CURRENT_BINARY_DIR}/constants/version.cpp
  util/buffer.cpp
  util/file.cpp
  util/logging.cpp
  util/mem.cpp
  util/str.cpp
  util/thread/queue_manager.cpp  
  util/thread/threading.cpp
  util/time.cpp
)

add_dependencies(lokinet-utils genversion)

lokinet_add_library(lokinet-contact
  contact/client_contact.cpp
  contact/client_intro.cpp
  contact/contactdb.cpp
  contact/relay_contact.cpp
  contact/router_id.cpp
  contact/sns.cpp
  contact/tag.cpp
)

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

  ev/fd_poller.cpp
  ev/tcp.cpp
)

lokinet_add_library(lokinet-dns
  dns/srv_data.cpp
)

if (LOKINET_FULL)
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
  target_sources(lokinet-dns PRIVATE
    dns/message.cpp   # dns/server
    dns/name.cpp      # srv_data, question, rr
    dns/platform.cpp
    dns/question.cpp  # message
    dns/rr.cpp
    dns/serialize.cpp
    dns/server.cpp
  )

  # platform specific bits and bobs for setting dns
  if(WITH_SYSTEMD)
    target_sources(lokinet-dns PRIVATE dns/nm_platform.cpp dns/sd_platform.cpp)
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
  config/config.cpp
  config/definition.cpp
  config/ini.cpp
)

# All path objects; link directly to lokinet-core
lokinet_add_library(lokinet-path
  path/build_stats.cpp
  path/path.cpp
  path/path_context.cpp
  path/path_handler.cpp
  path/path_types.cpp
  path/transit_hop.cpp
)

# Link libraries to their internals
target_link_libraries(lokinet-nodedb PUBLIC lokinet-addressing lokinet-cryptography)
target_link_libraries(lokinet-contact PUBLIC lokinet-dns)
if(LOKINET_FULL)
  target_link_libraries(lokinet-rpc PUBLIC lokinet-contact lokinet-config)
endif()
target_link_libraries(lokinet-addressing PUBLIC lokinet-utils lokinet-cryptography lokinet-contact lokinet-ip)
target_link_libraries(lokinet-config PUBLIC lokinet-cryptography lokinet-addressing)
if(LOKINET_FULL)
  target_link_libraries(lokinet-platform PUBLIC lokinet-cryptography lokinet-ip)
  target_link_libraries(lokinet-config PUBLIC lokinet-exit-auth)
endif()
target_link_libraries(lokinet-dns
  PUBLIC
  lokinet-cryptography
  lokinet-config
  lokinet-platform
)

target_link_libraries(lokinet-path
  PUBLIC
  lokinet-addressing
)

target_link_libraries(lokinet-core-utils 
  PUBLIC 
  lokinet-config
  lokinet-dns
)

if(LOKINET_FULL)
  target_link_libraries(lokinet-core-utils
    PUBLIC
    lokinet-rpc
    lokinet-platform
  )
endif()

target_link_libraries(lokinet-cryptography
  PUBLIC
  lokinet-libcrypt
)

if(LOKINET_FULL)
  target_link_libraries(lokinet-exit-auth PUBLIC lokinet-core-utils lokinet-cryptography)
endif()

target_link_libraries(lokinet-utils
  PUBLIC
  lokinet-cryptography
)

target_link_libraries(lokinet-core 
  PUBLIC 
  lokinet-core-utils
  lokinet-nodedb
  lokinet-path
)
if(LOKINET_FULL)
  target_link_libraries(lokinet-core PUBLIC lokinet-exit-auth)
endif()


target_link_libraries(lokinet-base 
  INTERFACE 

  Threads::Threads

  oxenc::oxenc 
  oxen::logging 
  
  sodium
)


if(LOKINET_FULL)
  target_link_libraries(lokinet-base INTERFACE libunbound oxenmq::oxenmq)
endif()

# libevent
if(NOT TARGET libevent::core)
  add_library(libevent_core INTERFACE)
  pkg_check_modules(LIBEVENT_core libevent_core>=2.1 IMPORTED_TARGET REQUIRED)
  target_link_libraries(libevent_core INTERFACE PkgConfig::LIBEVENT_core)
  add_library(libevent::core ALIAS libevent_core)
endif()
target_link_libraries(lokinet-addressing PRIVATE libevent::core)


add_library(liblokinet lokinet.cpp)
target_link_libraries(liblokinet PUBLIC lokinet-core)

set_target_properties(liblokinet PROPERTIES OUTPUT_NAME lokinet)
if(BUILD_SHARED_LIBS AND LOKINET_VERSION_SO)
  set_target_properties(liblokinet PROPERTIES
    VERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}"
    SOVERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}")
endif()

if(WIN32)
  target_link_libraries(liblokinet PUBLIC ws2_32 iphlpapi -fstack-protector)
  install(TARGETS liblokinet DESTINATION bin COMPONENT liblokinet)
elseif(NOT APPLE)
  include(GNUInstallDirs)
  install(TARGETS liblokinet LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT liblokinet)
endif()

add_library(lokinet::liblokinet ALIAS liblokinet)

file(GLOB_RECURSE docs_SRC */*.hpp *.hpp)
set(DOCS_SRC ${docs_SRC} PARENT_SCOPE)
