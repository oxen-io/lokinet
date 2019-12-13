#ifndef LLARP_RPC_HPP
#define LLARP_RPC_HPP

#include <util/time.hpp>

#include <functional>
#include <memory>
#include <string>

#include <naming/i_name_lookup_handler.hpp>

namespace llarp
{
  struct PubKey;
  struct AbstractRouter;

  namespace rpc
  {
    struct ServerImpl;

    /// jsonrpc server
    struct Server
    {
      Server(AbstractRouter* r);
      ~Server();

      bool
      Start(const std::string& bindaddr);

      /// stop and close
      void
      Stop();

     private:
      std::unique_ptr< ServerImpl > m_Impl;
    };

    struct CallerImpl;

    /// jsonrpc caller
    struct Caller : public llarp::naming::INameLookupHandler
    {
      Caller(AbstractRouter* r);
      ~Caller();

      /// set http basic auth for use with remote rpc endpoint
      void
      SetAuth(const std::string& user, const std::string& password);

      /// start with jsonrpc endpoint address
      bool
      Start(const std::string& remote);

      /// stop and close
      void
      Stop();

      /// do per second tick
      void
      Tick(llarp_time_t now);

      bool
      LookupNameAsync(const std::string name,
                      llarp::naming::NameLookupResultHandler h) override;

     private:
      std::unique_ptr< CallerImpl > m_Impl;
    };

  }  // namespace rpc
}  // namespace llarp

#endif
