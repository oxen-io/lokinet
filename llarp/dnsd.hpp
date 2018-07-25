#ifndef LIBLLARP_DNSD_HPP
#define LIBLLARP_DNSD_HPP

#include <llarp/ev.h>  // for sockaadr
#include <string>
#include "dns.hpp"  // question and dnsc
#include "dnsc.hpp"

struct dnsd_context;

typedef ssize_t (*sendto_dns_hook_func)(void *sock, const struct sockaddr *from,
                                        const void *buffer, size_t length);

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
  struct sockaddr *from;
  sendto_dns_hook_func hook;  // sendto hook tbh
  // maybe a reference to dnsd_context incase of multiple
  dnsd_context *context;  // or you can access it via user (udp)
};

// we could have passed in the source sockaddr in case you wanted to
// handle the response yourself
typedef sockaddr *(*intercept_query_hook)(std::string name);

struct dnsd_context
{
  /// DNS daemon socket to listen on
  struct llarp_udp_io udp;
  dnsc_context client;
  /// custom data for intercept query hook
  void *user;
  /// hook function for intercepting dns requests
  intercept_query_hook intercept;
};

void
llarp_handle_dnsd_recvfrom(struct llarp_udp_io *udp,
                           const struct sockaddr *paddr, const void *buf,
                           ssize_t sz);

/// initialize dns subsystem and bind socket
/// returns true on bind success otherwise returns false
bool
llarp_dnsd_init(struct dnsd_context *dnsd, struct llarp_ev_loop *netloop,
                const char *dnsd_ifname, uint16_t dnsd_port,
                const char *dnsc_hostname, uint16_t dnsc_port);

bool
llarp_dnsd_stop(struct dnsd_context *dnsd);

#endif
