#ifndef LLARP_DNS_H_
#define LLARP_DNS_H_

#include <llarp/ev.h>
#include <sys/types.h>  // for uint & ssize_t

#ifdef __cplusplus
extern "C"
{
#endif
  /**
   * dns.h
   *
   * dns client/server
   */

#define DNC_BUF_SIZE 512

  struct dns_query
  {
    uint16_t length;
    char *url;
    unsigned char request[DNC_BUF_SIZE];
    uint16_t reqType;
  };

  struct dns_client_request;

  typedef void (*resolve_dns_hook_func)(dns_client_request *request,
                                        struct sockaddr *);

  struct dns_client_request
  {
    /// sock type
    void *sock;
    /// customizeable (used for outer request)
    void *user;
    /// storage
    dns_query query;
    /// hook
    resolve_dns_hook_func resolved;
  };

  // forward declare
  struct dns_context;

  /// returns true if the dns query was intercepted
  typedef bool (*intercept_query_hook)(struct dns_context *, const dns_query *);

  /// context for dns subsystem
  struct dns_context
  {
    /// populated by llarp_dns_init
    struct llarp_udp_io udp;
    /// set by caller
    void *user;
    /// hook function for intercepting dns requests
    intercept_query_hook intercept;
  };

  struct sockaddr *
  resolveHost(const char *url);

  /// initialize dns subsystem and bind socket
  /// returns true on bind success otherwise returns false
  bool
  llarp_dns_init(struct dns_context *dns, struct llarp_ev_loop *loop,
                 const char *addr, uint16_t port);

  /// async resolve hostname
  bool
  llarp_resolve_host(struct dns_context *dns, const char *url,
                     resolve_dns_hook_func resolved, void *user);

  /*

  // XXX: these should be internal and not exposed

  void
  llarp_handle_recvfrom(struct llarp_udp_io *udp, const struct sockaddr *saddr,
                        const void *buf, ssize_t sz);
  void
  raw_handle_recvfrom(int *sockfd, const struct sockaddr *saddr, const void
  *buf, ssize_t sz);
  */

#ifdef __cplusplus
}
#endif
#endif
