#ifndef LLARP_NET_HPP
#define LLARP_NET_HPP
#include <string>
#include <sys/socket.h>

namespace llarp
{
  namespace net
  {
    bool GetIfAddr(const std::string & ifname, int af, sockaddr * addr);
  }
}

#endif
