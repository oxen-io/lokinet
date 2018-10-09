#ifndef LLARP_DNS_H_
#define LLARP_DNS_H_

//#include <llarp/ev.h>   // for sockaadr
//#include <sys/types.h>  // for uint & ssize_t

// all uint should have been removed in favor of uint_16t
/* non-cygnus does not have this type */
//#ifdef _WIN32
//#define uint UINT
//#endif

#ifdef __cplusplus
extern "C"
{
#endif
  /**
   * dns.h
   *
   * dns client/server
   */

  // fwd declr
  struct dnsc_answer_request;

  typedef void (*dnsc_answer_hook_func)(dnsc_answer_request *request);

#ifdef __cplusplus
}
#endif
#endif
