#ifndef LLARP_RPC_HPP
#define LLARP_RPC_HPP
#include <llarp/time.h>
#include <llarp/ev.h>
#include <string>
#include <functional>
#include <llarp/crypto.hpp>

// forward declare
struct llarp_router;

namespace llarp
{
  namespace rpc
  {
    struct ServerImpl;

    /// jsonrpc server
    struct Server
    {
      Server(llarp_router* r);
      ~Server();

      bool
      Start(const std::string& bindaddr);

     private:
      ServerImpl* m_Impl;
    };

    struct CallerImpl;

    /// jsonrpc caller
    struct Caller
    {
      Caller(llarp_router* r);
      ~Caller();

      /// start with jsonrpc endpoint address
      bool
      Start(const std::string& remote);

      /// async test if a router is valid via jsonrpc
      void
      AsyncVerifyRouter(llarp::PubKey pkey,
                        std::function< void(llarp::PubKey, bool) > handler);

     private:
      CallerImpl* m_Impl;
    };

  }  // namespace rpc
}  // namespace llarp

#endif
