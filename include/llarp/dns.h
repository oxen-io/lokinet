#ifndef LLARP_DNS_H_
#define LLARP_DNS_H_

#include <sys/types.h> // for uint & ssize_t
#include <llarp/ev.h>

/**
 * dns.h
 *
 * dns client/server
 */

#define DNC_BUF_SIZE 512

struct dns_query {
  uint16_t length;
  char * url;
  unsigned char request[DNC_BUF_SIZE];
  uint16_t reqType;
};

struct dns_client_request;

typedef void(*resolve_dns_hook_func)(dns_client_request *request, struct sockaddr *);

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

struct sockaddr *resolveHost(const char *url);
bool llarp_resolve_host(struct llarp_ev_loop *, const char *url,
                        resolve_dns_hook_func resolved, void *user);

void
llarp_handle_recvfrom(struct llarp_udp_io *udp, const struct sockaddr *saddr,
                const void *buf, ssize_t sz);
void
raw_handle_recvfrom(int *sockfd, const struct sockaddr *saddr,
           const void *buf, ssize_t sz);

#endif
