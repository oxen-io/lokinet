#ifndef LLARP_DNS_H_
#define LLARP_DNS_H_

#include <llarp/ev.h> // for sockaadr
#include <sys/types.h>  // for uint & ssize_t
#include <map> // for udp DNS tracker

/**
 * dns.h
 *
 * dns client/server
 */

//#include <mutex>
//typedef std::mutex mtx_t;
//typedef std::lock_guard< mtx_t > lock_t;

// fwd declr
//struct dns_query;
struct dnsc_context;
struct dnsd_context;
//struct dnsd_question_request;
struct dnsc_answer_request;

// dnsc can work over any UDP socket
// however we can't ignore udp->user
// we need to be able to reference the request (being a request or response)
// bottom line is we can't use udp->user
// so we'll need to track all incoming and outgoing requests

struct dns_tracker
{
  //uint c_responses;
  uint c_requests;
  std::map< uint, dnsc_answer_request * > client_request;
  // FIXME: support multiple dns server contexts
  dnsd_context *dnsd;
  //std::map< uint, dnsd_question_request * > daemon_request;
};

// should we pass by llarp::Addr
// not as long as we're supporting raw
typedef void (*dnsc_answer_hook_func)(dnsc_answer_request *request);

struct sockaddr *
raw_resolve_host(const char *url);

bool
llarp_resolve_host(struct dnsc_context *dns, const char *url,
                   dnsc_answer_hook_func resolved, void *user);
void
llarp_host_resolved(dnsc_answer_request *request);

#endif
