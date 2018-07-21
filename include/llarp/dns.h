#ifndef LLARP_DNS_H_
#define LLARP_DNS_H_

#include <llarp/ev.h>
#include <sys/types.h>  // for uint & ssize_t

/**
 * dns.h
 *
 * dns client/server
 */

#include <mutex>
typedef std::mutex mtx_t;
typedef std::lock_guard< mtx_t > lock_t;

#include <string>
struct dns_msg_header
{
  uint id;
  uint qr;
  uint opcode;
  uint aa;
  uint tc;
  uint rd;
  uint ra;
  uint rcode;

  uint qdCount;
  uint anCount;
  uint nsCount;
  uint arCount;
};

struct dns_msg_question
{
  std::string name;
  uint type;
  uint qClass;
};

struct dns_msg_answer
{
  std::string name;
  uint type;
  uint aClass;
  u_int32_t ttl;
  uint rdLen;
  uint rData;
};

// fwd declr
struct dns_query;
struct dnsc_context;
struct dnsd_context;
struct dns_request;
struct dns_client_request;

typedef bool (*intercept_query_hook)(struct dnsc_context *, const dns_query *);

// dnsc can work over any UDP socket
// however we can't ignore udp->user
// we need to be able to reference the request (being a request or response)
// bottom line is we can't use udp->user
// so we'll need to track all incoming and outgoing requests

#include <map>

struct dns_tracker
{
  uint c_responses;
  uint c_requests;
  std::map< uint, dns_client_request * > client_request;
  dnsd_context *dnsd;
  std::map< uint, dns_request * > daemon_request;
};

struct dnsc_context
{
  /// DNS server hostname to use
  // char *server;
  /// DNS server port to use
  // uint port;
  /// Target: DNS server hostname/port to use
  // FIXME: ipv6 it
  sockaddr *server;
  // where to create the new sockets
  // struct llarp_ev_loop *netloop;
  // FIXME: UDP socket pooling (or maybe at the libev level)
  struct llarp_udp_io *udp;
};

struct dnsd_context
{
  /// DNS daemon port to listen on
  struct llarp_udp_io udp;
  dnsc_context client;
  /// custom data for intercept query hook
  void *user;
  /// hook function for intercepting dns requests
  intercept_query_hook intercept;
};

/// initialize dns subsystem and bind socket
/// returns true on bind success otherwise returns false
bool
llarp_dnsc_init(struct dnsc_context *dnsc, struct llarp_udp_io *udp,
                const char *dnsc_hostname, uint16_t dnsc_port);

bool
llarp_dnsc_stop(struct dnsc_context *dnsc);

bool
llarp_dnsc_unbind(struct dns_client_request *request);

bool
llarp_dnsc_bind(struct llarp_udp_io *udp, struct dns_client_request *request);

/// initialize dns subsystem and bind socket
/// returns true on bind success otherwise returns false
bool
llarp_dnsd_init(struct dnsd_context *dns, struct llarp_ev_loop *netloop,
                const char *dnsd_ifname, uint16_t dnsd_port,
                const char *dnsc_hostname, uint16_t dnsc_port);

#define DNC_BUF_SIZE 512

struct dns_query
{
  uint16_t length;
  char *url;
  unsigned char request[DNC_BUF_SIZE];
  uint16_t reqType;
};

struct dns_client_request;

// should we pass by llarp::Addr
// not as long as we're supporting raw
typedef void (*resolve_dns_hook_func)(dns_client_request *request);

// FIXME: separate generic from llarp
struct dns_client_request
{
  /// sock type
  void *sock;  // pts to udp...
  /// customizeable (used for outer request)
  void *user;
  /// storage
  dns_query query;
  /// hook
  resolve_dns_hook_func resolved;
  /// result
  bool found;
  struct sockaddr result;
  /// Source: UDP port to use
  // FIXME: separate socket for now
  struct llarp_udp_io udp;
  // maybe a link to dnsc_context
};

bool
llarp_dnsc_bind(struct llarp_ev_loop *netloop,
                struct dns_client_request *request);

struct sockaddr *
resolveHost(const char *url);
bool
llarp_resolve_host(struct dnsc_context *dns, const char *url,
                   resolve_dns_hook_func resolved, void *user);
void
llarp_host_resolved(dns_client_request *request);

void
llarp_handle_recvfrom(struct llarp_udp_io *udp, const struct sockaddr *paddr,
                      const void *buf, ssize_t sz);

void
llarp_handle_dns_recvfrom(struct llarp_udp_io *udp,
                          const struct sockaddr *saddr, const void *buf,
                          ssize_t sz);

void
llarp_handle_recvfrom(struct llarp_udp_io *udp, const struct sockaddr *saddr,
                      const void *buf, ssize_t sz);

void
raw_handle_recvfrom(int *sockfd, const struct sockaddr *saddr, const void *buf,
                    ssize_t sz);

#endif
