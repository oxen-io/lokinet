#ifndef LLARP_DNSC_HPP
#define LLARP_DNSC_HPP

#include <dns.hpp>  // get protocol structs
#include <ev/ev.h>  // for sockaadr

// internal, non-public functions
// well dnsc init/stop are public...

struct dnsc_answer_request;

#define DNC_BUF_SIZE 512
/// a question to be asked remotely (the actual bytes to send on the wire)
// header, question
struct dns_query
{
  uint16_t length;
  unsigned char request[DNC_BUF_SIZE];
  // char *url;
  // uint16_t reqType;
};

struct dns_query *
build_dns_packet(char *url, uint16_t id, uint16_t reqType);

/// hook function to handle an dns client request
// should we pass by llarp::Addr
// not as long as we're supporting raw
using dnsc_answer_hook_func = void (*)(dnsc_answer_request *);

/// struct for dns client requests
struct dnsc_answer_request
{
  /// sock type
  void *sock;  // points to udp that sent the request to DNSc...
  /// customizable (used for hook (outer request))
  void *user;
  /// request storage
  dns_msg_question question;
  /// response storage
  dns_packet packet;
  /// hook
  dnsc_answer_hook_func resolved;
  /// result
  bool found;

  // llarp::huint32_t result;
  // std::string revDNS;

  // a reference to dnsc_context incase of multiple contexts
  struct dnsc_context *context;
};

/// event handler for processing DNS responses
void
llarp_handle_dnsc_recvfrom(struct llarp_udp_io *const udp,
                           const struct sockaddr *addr, ManagedBuffer buf);

/// generic handler for processing DNS responses
/// this doesn't look like it exists
/// that's because raw_resolve_host calls generic_handle_dnsc_recvfrom directly
/// because we don't need a callback like recvfrom
/// because we're not evented
/// however daemon/dns expects this
/*
void
raw_handle_recvfrom(int *sockfd, const struct sockaddr *addr, const void *buf,
                    const ssize_t sz);
*/

// removed saddr, if needed get through request
void
generic_handle_dnsc_recvfrom(dnsc_answer_request *request,
                             const llarp_buffer_t &buffer, dns_msg_header *hdr);

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
  llarp::Logic *logic;
};

/// async (blocking w/callback) resolve a hostname using generic socks
void
raw_resolve_host(struct dnsc_context *const dnsc, const char *url,
                 dnsc_answer_hook_func resolved, void *const user,
                 uint16_t type);

/// async (non blocking w/callback) resolve a hostname using llarp platform
/// framework
bool
llarp_resolve_host(struct dnsc_context *const dns, const char *url,
                   dnsc_answer_hook_func resolved, void *const user,
                   uint16_t type);

/// cleans up request structure allocations
void
llarp_host_resolved(dnsc_answer_request *const request);

/// initialize dns subsystem and bind socket
/// returns true on bind success otherwise returns false
bool
llarp_dnsc_init(struct dnsc_context *const dnsc, llarp::Logic *const logic,
                struct llarp_ev_loop *const netloop,
                const llarp::Addr &dnsc_sockaddr);

/// shutdowns any events, and deallocates for this context
bool
llarp_dnsc_stop(struct dnsc_context *const dnsc);

#endif
