#include "link.hpp"
#include <cstring>

bool operator < (const sockaddr_in6 addr0, const sockaddr_in6 addr1)
{
  return memcmp(addr0.sin6_addr.s6_addr, addr1.sin6_addr.s6_addr, sizeof(addr0.sin6_addr)) && addr0.sin6_port < addr1.sin6_port;
}

namespace sarp
{

  int Link::Run()
  {
    uint8_t recvbuff[1500];
    sockaddr_in6 remote;
    socklen_t remotelen;
    ssize_t ret = 0;
    do
    {
      ret = recvfrom(sockfd, recvbuff, sizeof(recvbuff),0, (sockaddr *) &remote, &remotelen);
      if(ret > 0)
      {
        auto itr = sessions.find(remote);
        if(itr == sessions.end())
        {
          sessions[remote] = std::make_unique<PeerSession>(crypto, remote);
        }
        sessions[remote]->RecvFrom(recvbuff, ret);
      }
    }
    while(ret != -1);
    return -1;
  }
}
