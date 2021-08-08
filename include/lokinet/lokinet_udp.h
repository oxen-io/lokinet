#pragma once

#include "lokinet_context.h"

#ifdef _WIN32
extern "C"
{
  struct iovec
  {
    void* iov_base;
    size_t iov_len;
  };
}
#else
#include <sys/uio.h>
#endif

#ifdef __cplusplus
extern "C"
{
#endif

  /// information about a udp flow
  struct lokinet_udp_flow
  {
    /// the socket id for this flow used for i/o purposes and closing this socket
    int socket_id;
    /// remote endpoint's .loki or .snode address
    char remote_addr[256];
    /// local endpoint's ip address
    char local_addr[64];
    /// remote endpont's port
    int remote_port;
    /// local endpoint's port
    int local_port;
  };

  /// information about a udp endpoint
  struct lokinet_udp_addr
  {
    /// the peer's .loki or .snode address
    char addr[256];
    /// the peer's "real" port
    int port;
  };

  /// establish an outbound udp flow
  /// remoteAddr is the remote host:port tuple we are making a flow for
  /// localAddr is the local ip:port to bind our socket to.
  /// flow will be populated with this flow's information
  /// return 0 on success return nonzero on fail containing an errno value
  int EXPORT
  lokinet_udp_establish(
      char* remoteAddr,
      char* localAddr,
      struct lokinet_udp_flow* flow,
      struct lokinet_context* ctx);

  /// a result from a lokinet_udp_bind call
  struct lokinet_udp_bind_result
  {
    /// a socket id used to close a lokinet udp socket
    int socket_id;
    /// local socket address
    struct lokinet_udp_addr sockname;
  };

  /// inbound listen udp socket
  /// expose udp port exposePort to the network, any .loki traffic for udp on that port will be
  /// forwarded localAddr to forward inbound udp packets to "ip:port" returns 0 on success returns
  /// nonzero on error in which it is an errno value
  int EXPORT
  lokinet_udp_bind(
      int exposePort,
      char* localAddr,
      struct lokinet_udp_listen_result* result,
      struct lokinet_context* ctx);

  /// look up the remote endpoint of a local udp mapping for inbound sockets
  int EXPORT
  lokinet_udp_peername(
      int socket_id, char* localaddr, struct lokinet_udp_addr* peer, struct lokinet_context* ctx);

#ifdef __cplusplus
}
#endif
