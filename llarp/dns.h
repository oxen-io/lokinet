#ifndef LLARP_DNS_H_
#define LLARP_DNS_H_

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
