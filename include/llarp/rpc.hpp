#ifndef LLARP_RPC_HPP
#define LLARP_RPC_HPP
#include <llarp/time.h>
#include <llarp/ev.h>
#include <string>

// forward declare
struct llarp_router;

namespace llarp
{
  namespace rpc
  {
    struct ServerImpl;

    struct Server
    {
      Server(llarp_router* r);
      ~Server();

      bool
      Start(const std::string& bindaddr);

     private:
      ServerImpl* m_Impl;
    };

  }  // namespace rpc
}  // namespace llarp

#endif
