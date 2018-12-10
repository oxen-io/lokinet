#ifndef LLARP_IWP_HPP
#define LLARP_IWP_HPP
#include <llarp/crypto.h>
#include <string>

struct llarp_iwp_args
{
  struct llarp_crypto* crypto;
  llarp::Logic* logic;
  struct llarp_threadpool* cryptoworker;
  struct llarp_router* router;
  bool permitInbound;
};

#endif
