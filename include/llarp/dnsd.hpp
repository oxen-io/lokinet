#ifndef LIBLLARP_DNSD_HPP
#define LIBLLARP_DNSD_HPP

#include <llarp/ev.h>  // for sockaadr
#include <string>
#include <llarp/dns.hpp>  // question and dnsc
#include <llarp/dnsc.hpp>

//
// Input structures/functions:
//

// fwd declaration
struct dnsd_context;

/// sendto hook functor
using sendto_dns_hook_func =
    std::function< ssize_t(void *sock, const struct sockaddr *from,
                           const void *buffer, size_t length) >;
// FIXME: llarp::Addr

/// DNS server query request
struct dnsd_question_request
{
  /// sock type
  void *user;
  // raw or llarp subsystem (is this used? does this matter?)
  bool llarp;
  /// request id
  unsigned int id;
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
  llarp::huint32_t *returnThis;
};

/// builds and fires a request based based on llarp_udp_io udp event
/// called by the llarp_handle_dns_recvfrom generic (dnsd/dnsc) handler in dns
void
llarp_handle_dnsd_recvfrom(struct llarp_udp_io *udp,
                           const struct sockaddr *addr, const void *buf,
                           ssize_t sz);

//
// output structures/functions:
//
// we may want to pass dnsd_question_request to these,
//   incase we need to send an error back up through the pipeline

/// NXDOMAIN not found
void
write404_dnss_response(const struct sockaddr *from,
                       dnsd_question_request *request);

/// for hook functions to use
void
writecname_dnss_response(std::string cname, const struct sockaddr *from,
                         dnsd_question_request *request);
// FIXME: llarp::Addr

/// send an A record found response
void
writesend_dnss_response(llarp::huint32_t *hostRes, const struct sockaddr *from,
                        dnsd_question_request *request);
// FIXME: llarp::Addr

/// send an PTR record found response
void
writesend_dnss_revresponse(std::string reverse, const struct sockaddr *from,
                           dnsd_question_request *request);
// FIXME: llarp::Addr

//
// setup/teardown functions/structure:
//

/// intercept query hook functor
using intercept_query_hook = std::function< dnsd_query_hook_response *(
    std::string name, const struct sockaddr *from,
    struct dnsd_question_request *request) >;
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

/// initialize dns subsystem and bind socket
/// returns true on bind success otherwise returns false
bool
llarp_dnsd_init(struct dnsd_context *const dnsd,
                struct llarp_logic *const logic,
                struct llarp_ev_loop *const netloop,
                const llarp::Addr &dnsd_sockaddr,
                const llarp::Addr &dnsc_sockaddr);

/// shutdowns any events, and deallocates for this context
bool
llarp_dnsd_stop(struct dnsd_context *const dnsd);

#endif
