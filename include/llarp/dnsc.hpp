#ifndef LIBLLARP_DNSC_HPP
#define LIBLLARP_DNSC_HPP

#include <llarp/ev.h>     // for sockaadr
#include <llarp/dns.hpp>  // get protocol structs

// internal, non-public functions
// well dnsc init/stop are public...

struct dnsc_answer_request;

#define DNC_BUF_SIZE 512
/// a question to be asked remotely (the actual bytes to send on the wire)
// header, question
struct dns_query
{
  uint16_t length;
  // char *url;
  unsigned char request[DNC_BUF_SIZE];
  // uint16_t reqType;
};

struct dns_query *
build_dns_packet(char *url, uint16_t id, uint16_t reqType);

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
  llarp::Addr result;
  std::string revDNS;
  // a reference to dnsc_context incase of multiple contexts
  struct dnsc_context *context;
};

/// event handler for processing DNS responses
void
llarp_handle_dnsc_recvfrom(struct llarp_udp_io *const udp,
                           const struct sockaddr *addr, const void *buf,
                           const ssize_t sz);

/// generic handler for processing DNS responses
/// this doesn't look like it exists
/// that's because raw_resolve_host calls generic_handle_dnsc_recvfrom directly
/// because we don't need a callback like recvfrom
/// because we're not evented
/// however daemon/dns expects this
void
raw_handle_recvfrom(int *sockfd, const struct sockaddr *addr, const void *buf,
                    const ssize_t sz);

/// DNS client context (one needed per upstream DNS server)
struct dnsc_context
{
  /// Target: DNS servers to use
  std::vector< llarp::Addr > resolvers;
  /// udp tracker
  struct dns_tracker *tracker;
  /// sock type
  void *sock;
  // where to create the new sockets
  struct llarp_udp_io *udp;
  /// We will likely need something for timing events (timeouts)
  struct llarp_logic *logic;
};

/// async resolve a hostname using generic socks
void
raw_resolve_host(struct dnsc_context *const dnsc, const char *url,
                 dnsc_answer_hook_func resolved, void *const user);

/// async resolve a hostname using llarp platform framework
bool
llarp_resolve_host(struct dnsc_context *const dns, const char *url,
                   dnsc_answer_hook_func resolved, void *const user);

/// cleans up request structure allocations
void
llarp_host_resolved(dnsc_answer_request *const request);

/// initialize dns subsystem and bind socket
/// returns true on bind success otherwise returns false
bool
llarp_dnsc_init(struct dnsc_context *const dnsc,
                struct llarp_logic *const logic,
                struct llarp_ev_loop *const netloop,
                const llarp::Addr &dnsc_sockaddr);

/// shutdowns any events, and deallocates for this context
bool
llarp_dnsc_stop(struct dnsc_context *const dnsc);

#endif
