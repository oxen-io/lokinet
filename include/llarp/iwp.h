#ifndef LLARP_IWP_H_
#define LLARP_IWP_H_
#include <llarp/crypto.h>
#include "router.hpp"

struct llarp_iwp_args
{
  struct llarp_crypto* crypto;
  struct llarp_logic* logic;
  struct llarp_threadpool* cryptoworker;
  struct llarp_router* router;
  const char* keyfile;
};

#endif
