#ifndef LLARP_RPC_HPP
#define LLARP_RPC_HPP

#include <util/time.hpp>

#include <string>
#include <functional>

namespace llarp
{
  struct PubKey;
  struct Router;

  namespace rpc
  {
    struct ServerImpl;

    /// jsonrpc server
    struct Server
    {
      Server(Router* r);
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
      Caller(Router* r);
      ~Caller();

      /// set http basic auth for use with remote rpc endpoint
      void
      SetBasicAuth(const std::string& user, const std::string& password);

      /// start with jsonrpc endpoint address
      bool
      Start(const std::string& remote);

      /// stop and close
      void
      Stop();

      /// do per second tick
      void
      Tick(llarp_time_t now);

     private:
      CallerImpl* m_Impl;
    };

  }  // namespace rpc
}  // namespace llarp

#endif
