#ifndef LLARP_DNS_H_
#define LLARP_DNS_H_

#include <llarp/ev.h>   // for sockaadr
#include <sys/types.h>  // for uint & ssize_t

/* non-cygnus does not have this type */
#ifdef _WIN32
#define uint UINT
#endif

#ifdef __cplusplus
extern "C"
{
#endif
  /**
   * dns.h
   *
   * dns client/server
   */

  //#include <mutex>
  // typedef std::mutex mtx_t;
  // typedef std::lock_guard< mtx_t > lock_t;

  // fwd declr
  // struct dns_query;
  struct dnsc_context;
  struct dnsd_context;
  // struct dnsd_question_request;
  struct dnsc_answer_request;

  //struct dns_tracker;

  // should we pass by llarp::Addr
  // not as long as we're supporting raw
  typedef void (*dnsc_answer_hook_func)(dnsc_answer_request *request);

#ifdef __cplusplus
}
#endif
#endif
