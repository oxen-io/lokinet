#ifndef LIBLLARP_DNSC_HPP
#define LIBLLARP_DNSC_HPP

#include <llarp/ev.h> // for sockaadr
#include "dns.hpp" // get protocol structs

// internal, non-public functions
// well dnsc init/stop are public...

struct dnsc_answer_request;

// should we pass by llarp::Addr
// not as long as we're supporting raw
typedef void (*dnsc_answer_hook_func)(dnsc_answer_request *request);

// FIXME: separate generic from llarp
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

void
llarp_handle_dnsc_recvfrom(struct llarp_udp_io *udp,
                           const struct sockaddr *saddr, const void *buf,
                           ssize_t sz);

void
raw_handle_recvfrom(int *sockfd, const struct sockaddr *saddr, const void *buf,
                    ssize_t sz);

struct dnsc_context
{
  /// Target: DNS server hostname/port to use
  // FIXME: ipv6 it
  sockaddr *server;
  // where to create the new sockets
  struct llarp_udp_io *udp;
};

/// initialize dns subsystem and bind socket
/// returns true on bind success otherwise returns false
bool
llarp_dnsc_init(struct dnsc_context *dnsc, struct llarp_udp_io *udp,
                const char *dnsc_hostname, uint16_t dnsc_port);

bool
llarp_dnsc_stop(struct dnsc_context *dnsc);

#endif
