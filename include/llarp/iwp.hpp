#ifndef LLARP_IWP_HPP
#define LLARP_IWP_HPP
#include <llarp/crypto.h>
#include <string>

namespace llarp {
    class Logic;
    struct Router;
}

struct llarp_iwp_args
{
  struct llarp::Crypto* crypto;
  llarp::Logic* logic;
  struct llarp_threadpool* cryptoworker;
  struct llarp::Router* router;
  bool permitInbound;
};

#endif
