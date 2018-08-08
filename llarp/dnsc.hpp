#ifndef LIBLLARP_DNSC_HPP
#define LIBLLARP_DNSC_HPP

#include <llarp/ev.h>  // for sockaadr
#include "dns.hpp"     // get protocol structs

// internal, non-public functions
// well dnsc init/stop are public...

struct dnsc_answer_request;

/// hook function to handle an dns client request
// should we pass by llarp::Addr
// not as long as we're supporting raw
typedef void (*dnsc_answer_hook_func)(dnsc_answer_request *request);

/// struct for dns client requests
struct dnsc_answer_request
{
  /// sock type
  void *sock;  // pts to udp...
  /// customizable (used for hook (outer request))
  void *user;
  /// storage
  dns_msg_question question;
  /// hook
  dnsc_answer_hook_func resolved;
  /// result
  bool found;
  struct sockaddr result;
  // a reference to dnsc_context incase of multiple contexts
  struct dnsc_context *context;
};

/// event handler for processing DNS responses
void
llarp_handle_dnsc_recvfrom(struct llarp_udp_io *udp,
                           const struct sockaddr *saddr, const void *buf,
                           ssize_t sz);

/// generic handler for processing DNS responses
void
raw_handle_recvfrom(int *sockfd, const struct sockaddr *saddr, const void *buf,
                    ssize_t sz);

/// DNS client context (one needed per upstream DNS server)
struct dnsc_context
{
  /// Target: DNS server hostname/port to use
  // FIXME: ipv6 it & make it a vector
  sockaddr *server;
  /// tracker
  struct dns_tracker *tracker;
  /// sock type
  void *sock;
  // where to create the new sockets
  struct llarp_udp_io *udp;
};

/// async resolve a hostname using generic socks
void
raw_resolve_host(struct dnsc_context *dnsc, const char *url,
                 dnsc_answer_hook_func resolved, void *user);

/// async resolve a hostname using llarp platform framework
bool
llarp_resolve_host(struct dnsc_context *dns, const char *url,
                   dnsc_answer_hook_func resolved, void *user);

/// cleans up request structure allocations
void
llarp_host_resolved(dnsc_answer_request *request);

/// initialize dns subsystem and bind socket
/// returns true on bind success otherwise returns false
bool
llarp_dnsc_init(struct dnsc_context *dnsc, struct llarp_udp_io *udp,
                const char *dnsc_hostname, uint16_t dnsc_port);

/// shutdowns any events, and deallocates for this context
bool
llarp_dnsc_stop(struct dnsc_context *dnsc);

#endif
