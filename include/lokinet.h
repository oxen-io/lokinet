#ifndef LOKINET_H
#define LOKINET_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  struct lokinet_context;

  /// allocate a new lokinet context
  struct lokinet_context*
  lokinet_context_new();

  /// free a context allocated by lokinet_context_new
  void
  lokinet_context_free(struct lokinet_context*);

  /// spawn all the threads needed for operation and start running
  void
  lokinet_context_start(struct lokinet_context*);

  /// stop all operations on this lokinet context
  void
  lokinet_context_stop(struct lokinet_context*);

  /// get default lokinet context
  /// does not need to be freed by lokinet_context_free
  struct lokinet_context*
  lokinet_default();

  /// the result of a lokinet stream mapping attempt
  struct lokinet_stream_result
  {
    /// set to zero on success otherwise the error that happened
    /// use strerror(3) to get printable string of this error
    int errno;

    /// the local ip address we mapped the remote endpoint to
    char* local_address;
    /// the local port we mapped the remote endpoint to
    int local_port;
  };

  /// connect out to a remote endpoint
  /// remote is in the form of "name:port"
  /// returns NULL if context was NULL or not started
  /// returns a free()-able lokinet_stream_result * that contains the result
  struct lokinet_stream_result*
  lokinet_outbound_stream(const char* remote, struct lokinet_context* context);

  /// stream accept filter determines if we should accept a stream or not
  /// return 0 to accept
  /// return -1 to explicitly reject
  /// return -2 to silently drop
  typedef int (*lokinet_stream_filter)(const char*, uint16_t, struct sockaddr* const, void*);

  /// set stream accepter filter
  /// passes user parameter into stream filter as void *
  void
  lokinet_inbound_stream_filter(
      lokinet_stream_filter acceptFilter, void* user, struct lokinet_context* context);

  /// simple stream acceptor
  /// simple variant of lokinet_inbound_stream_filter that maps port to localhost:port
  void
  lokinet_inbound_stream(uint16_t port, struct lokinet_context* context);

#ifdef __cplusplus
}
#endif
#endif
