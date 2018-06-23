#ifndef LLARP_API_CLIENT_HPP
#define LLARP_API_CLIENT_HPP

#include <string>

namespace llarp
{
  namespace api
  {
    struct ClientPImpl;

    struct Client
    {
      Client();
      ~Client();

      bool
      Start(const std::string& apiURL);

      int
      Mainloop();

     private:
      ClientPImpl* m_Impl;
    };

  }  // namespace api
}  // namespace llarp
#endif