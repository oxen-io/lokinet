#ifndef LIBLLARP_DNSD_HPP
#define LIBLLARP_DNSD_HPP

#include <llarp/ev.h>  // for sockaadr
#include <string>
#include "dns.hpp"  // question and dnsc
#include "dnsc.hpp"

// fwd declaration
struct dnsd_context;

/// sendto hook functor
typedef ssize_t (*sendto_dns_hook_func)(void *sock, const struct sockaddr *from,
                                        const void *buffer, size_t length);
// FIXME: llarp::Addr

/// DNS server query request
struct dnsd_question_request
{
  /// sock type
  void *user;
  // raw or llarp subsystem
  bool llarp;
  /// request id
  int id;
  /// question being asked
  dns_msg_question question;
  // request source socket
  struct sockaddr *from;             // FIXME: convert to llarp::Addr
  sendto_dns_hook_func sendto_hook;  // sendto hook tbh
  // maybe a reference to dnsd_context incase of multiple
  dnsd_context *context;  // or you can access it via user (udp)
};

// FIXME: made better as a two way structure, collapse the request and response
// together
struct dnsd_query_hook_response
{
  /// turn off communication
  bool dontSendResponse;
  /// turn off recursion
  bool dontLookUp;
  /// potential address
  sockaddr *returnThis;  // FIXME: llarp::Addr
};

/// intercept query hook functor
typedef dnsd_query_hook_response *(*intercept_query_hook)(
    std::string name, const struct sockaddr *from,
    struct dnsd_question_request *request);
// FIXME: llarp::Addr

/// DNS Server context
struct dnsd_context
{
  /// DNS daemon socket to listen on
  struct llarp_udp_io udp;
  /// udp tracker
  struct dns_tracker *tracker;
  /// upstream DNS client context to use
  dnsc_context client;
  /// custom data for intercept query hook (used for configuration of hook)
  void *user;
  /// hook function for intercepting dns requests
  intercept_query_hook intercept;
};

/// udp event handler
void
llarp_handle_dnsd_recvfrom(struct llarp_udp_io *udp,
                           const struct sockaddr *addr, const void *buf,
                           ssize_t sz);

/// for hook functions to use
void
writecname_dnss_response(std::string cname, const struct sockaddr *from,
                         dnsd_question_request *request);
// FIXME: llarp::Addr

void
writesend_dnss_response(struct sockaddr *hostRes, const struct sockaddr *from,
                        dnsd_question_request *request);
// FIXME: llarp::Addr

/// initialize dns subsystem and bind socket
/// returns true on bind success otherwise returns false
bool
llarp_dnsd_init(struct dnsd_context *dnsd, struct llarp_logic *logic,
                struct llarp_ev_loop *netloop, const llarp::Addr &dnsd_sockaddr,
                const llarp::Addr &dnsc_sockaddr);

/// shutdowns any events, and deallocates for this context
bool
llarp_dnsd_stop(struct dnsd_context *dnsd);

#endif
