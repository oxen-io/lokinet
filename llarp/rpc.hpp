#ifndef LLARP_RPC_HPP
#define LLARP_RPC_HPP

#include <crypto.hpp>
#include <ev.h>
#include <time.hpp>

#include <string>
#include <functional>

namespace llarp
{
  struct Router;

  namespace rpc
  {
    struct ServerImpl;

    /// jsonrpc server
    struct Server
    {
      Server(llarp::Router* r);
      ~Server();

      bool
      Start(const std::string& bindaddr);

      /// stop and close
      void
      Stop();

     private:
      ServerImpl* m_Impl;
    };

    struct CallerImpl;

    /// jsonrpc caller
    struct Caller
    {
      Caller(llarp::Router* r);
      ~Caller();

      /// start with jsonrpc endpoint address
      bool
      Start(const std::string& remote);

      /// stop and close
      void
      Stop();

      /// test if a router is valid
      bool
      VerifyRouter(const llarp::PubKey& pk);

      /// do per second tick
      void
      Tick(llarp_time_t now);

     private:
      CallerImpl* m_Impl;
    };

  }  // namespace rpc
}  // namespace llarp

#endif
