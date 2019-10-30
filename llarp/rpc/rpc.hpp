#ifndef LLARP_RPC_HPP
#define LLARP_RPC_HPP

#include <util/time.hpp>

#include <functional>
#include <memory>
#include <string>

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
      Start();

      /// stop and close
      void
      Stop();

      /// tick
      void
      Tick(llarp_time_t now);

     private:
      std::unique_ptr< ServerImpl > m_Impl;
    };

  }  // namespace rpc
}  // namespace llarp

#endif
