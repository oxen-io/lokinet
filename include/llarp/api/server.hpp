#ifndef LLARP_API_SERVER_HPP
#define LLARP_API_SERVER_HPP

#include <llarp/ev.h>
#include <llarp/router.h>
#include <string>

namespace llarp
{
  namespace api
  {
    struct ServerPImpl;

    struct Server
    {
      Server(llarp_router* r);
      ~Server();

      bool
      Bind(const std::string& url, llarp_ev_loop* loop);

     private:
      ServerPImpl* m_Impl;
    };

  }  // namespace api
}  // namespace llarp

#endif