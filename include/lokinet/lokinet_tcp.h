#pragma once

#include "lokinet_context.h"

#ifdef __cplusplus
extern "C"
{
#endif

  /// the result of a lokinet tcp mapping attempt
  struct lokinet_tcp_result
  {
    /// set to zero on success otherwise the error that happened
    /// use strerror(3) to get printable string of this error
    int error{0};
    /// set once the tcp connection is successfully established
    bool success{false};
    /// the local ip address we mapped the remote endpoint to
    /// null terminated
    char local_address[256];
    /// the local port we mapped the remote endpoint to
    int local_port;
    /// the id (aka: 'pseudo-port') of the tcp we created
    int tcp_id;
  };

  /// connect out to a remote endpoint
  /// remoteAddr is in the form of "name:port"
  /// localAddr is either NULL for any or in the form of "ip:port" to bind to an explicit address
  void EXPORT
  lokinet_outbound_tcp(
      struct lokinet_tcp_result* result,
      const char* remoteAddr,
      const char* localAddr,
      struct lokinet_context* context,
      void (*open_cb)(bool success, void* user_data),
      void (*close_cb)(int rv, void* user_data),
      void* user_data);

  /// tcp accept filter determines if we should accept a tcp or not
  /// return 0 to accept
  /// return -1 to explicitly reject
  /// return -2 to silently drop
  typedef int (*lokinet_tcp_filter)(const char* remote, uint16_t port, void* userdata);

  /// set tcp accepter filter
  /// passes user parameter into tcp filter as void *
  /// returns tcp id
  int EXPORT
  lokinet_inbound_tcp_filter(
      lokinet_tcp_filter acceptFilter, void* user, struct lokinet_context* context);

  /// simple tcp acceptor
  /// simple variant of lokinet_inbound_tcp_filter that maps port to localhost:port
  int EXPORT
  lokinet_inbound_tcp(uint16_t port, struct lokinet_context* context);

  void EXPORT
  lokinet_close_tcp(int tcp_id, struct lokinet_context* context);

#ifdef __cplusplus
}
#endif
