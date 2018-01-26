#include "link.hpp"
#include <cstring>
#include <llarp/time.h>

bool operator < (const sockaddr_in6 addr0, const sockaddr_in6 addr1)
{
  return memcmp(addr0.sin6_addr.s6_addr, addr1.sin6_addr.s6_addr, sizeof(addr0.sin6_addr)) && addr0.sin6_port < addr1.sin6_port;
}

namespace llarp
{

  static void link_recv_from(struct llarp_udp_listener * l, const struct sockaddr * src, char * buff, ssize_t sz)
  {
    if(src && src->sa_family == AF_INET6)
    {
      Link * link = static_cast<Link*>(l->user);
      struct sockaddr_in6 remote;
      memcpy(&remote, src, sizeof(sockaddr_in6));
      auto itr = link->sessions.find(remote);
      if(itr == link->sessions.end())
      {
        link->sessions[remote] = std::make_unique<PeerSession>(link->_crypto, remote);
      }
      link->sessions[remote]->RecvFrom(buff, sz);
    }
  }
  
  Link::Link(llarp_crypto * crypto) : _crypto(crypto)
  {
    _listener.user = this;
    _listener.recvfrom = link_recv_from;
  }

  Link::~Link()
  {
  }
  
  PeerSession::PeerSession(llarp_crypto * crypto, sockaddr_in6 remote) :
    lastRX(0),
    remoteAddr(remote),
    _crypto(crypto),
    state(eHandshakeInboundInit)
  {
    memset(sessionKey, 0, sizeof(sessionKey));
  }

  void PeerSession::RecvFrom(const char * buff, ssize_t sz)
  {
    lastRX = llarp_time_now_ms();
  }
}
