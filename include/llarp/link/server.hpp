#ifndef LLARP_LINK_SERVER_HPP
#define LLARP_LINK_SERVER_HPP
#include <unordered_map>
#include <llarp/threading.hpp>
#include <llarp/router_contact.hpp>
#include <llarp/crypto.hpp>
#include <llarp/net.hpp>
#include <llarp/ev.h>
#include <llarp/link/session.hpp>

struct llarp_router;

namespace llarp
{
  struct ILinkLayer
  {
    ILinkLayer(llarp_router* r) : m_router(r)
    {
    }

    bool
    HasSessionTo(const PubKey& pk)
    {
      util::Lock l(m_LinksMutex);
      return m_Links.find(pk) != m_Links.end();
    }

    bool
    HasSessionVia(const Addr& addr)
    {
      util::Lock l(m_SessionsMutex);
      return m_Sessions.find(addr) != m_Sessions.end();
    }

    static void
    udp_tick(llarp_udp_io* udp)
    {
      static_cast< ILinkLayer* >(udp->user)->Tick();
    }

    static void
    udp_recv_from(llarp_udp_io* udp, const sockaddr* from, const void* buf,
                  const ssize_t sz)
    {
      static_cast< ILinkLayer* >(udp->user)->RecvFrom(*from, buf, sz);
    }

    bool
    Configure(llarp_ev_loop* loop, const std::string& ifname, int af,
              uint16_t port)
    {
      m_udp.user     = this;
      m_udp.recvfrom = &ILinkLayer::udp_recv_from;
      m_udp.tick     = &ILinkLayer::udp_tick;
      if(ifname == "*")
      {
        if(!AllInterfaces(af, m_ourAddr))
          return false;
      }
      else if(!GetIFAddr(ifname, m_ourAddr, af))
        return false;
      return llarp_ev_add_udp(loop, &m_udp, m_ourAddr) != -1;
    }

    virtual ILinkSession*
    NewInboundSession(const Addr& from) const = 0;

    virtual ILinkSession*
    NewOutboundSession(const RouterContact& rc) const = 0;

    void
    Tick()
    {
      auto now = llarp_time_now_ms();
      util::Lock l(m_SessionsMutex);
      auto itr = m_Sessions.begin();
      while(itr != m_Sessions.end())
      {
        itr->second->Tick(now);
        if(itr->second->TimedOut(now))
          itr = m_Sessions.erase(itr);
        else
          ++itr;
      }
    }

    void
    RecvFrom(const Addr& from, const void* buf, size_t sz)
    {
      util::Lock l(m_SessionsMutex);
      auto itr = m_Sessions.find(from);
      if(itr == m_Sessions.end())
        m_Sessions
            .insert(std::make_pair(
                from, std::unique_ptr< ILinkSession >(NewInboundSession(from))))
            .first->second->Recv(buf, sz);
      else
        itr->second->Recv(buf, sz);
    }

    virtual bool
    PickAddress(const RouterContact& rc, llarp::Addr& picked) const = 0;

    void
    TryEstablishTo(const RouterContact& rc)
    {
      llarp::Addr to;
      if(!PickAddress(rc, to))
        return;
      util::Lock l(m_SessionsMutex);
      auto itr = m_Sessions.find(to);
      if(itr == m_Sessions.end())
        m_Sessions
            .insert(std::make_pair(
                to, std::unique_ptr< ILinkSession >(NewOutboundSession(rc))))
            .first->second->Handshake();
      else
        itr->second->Handshake();
    }

    virtual bool
    Start(llarp_logic* l) = 0;

    virtual void
    Stop() = 0;

    virtual const char*
    Name() const = 0;

    void
    CloseSessionTo(const PubKey& remote)
    {
      llarp::Addr addr;
      {
        util::Lock l(m_LinksMutex);
        auto itr = m_Links.find(remote);
        if(itr == m_Links.end())
          return;
        addr = itr->second;
      }
      {
        util::Lock l(m_SessionsMutex);
        auto itr = m_Sessions.find(addr);
        if(itr == m_Sessions.end())
          return;
        itr->second->SendClose();
        m_Sessions.erase(itr);
      }
    }

    void
    KeepAliveSessionTo(const PubKey& remote)
    {
      llarp::Addr addr;
      {
        util::Lock l(m_LinksMutex);
        auto itr = m_Links.find(remote);
        if(itr == m_Links.end())
          return;
        addr = itr->second;
      }
      {
        util::Lock l(m_SessionsMutex);
        auto itr = m_Sessions.find(addr);
        if(itr == m_Sessions.end())
          return;
        itr->second->SendKeepAlive();
      }
    }

    bool
    SendTo(const PubKey& remote, llarp_buffer_t buf)
    {
      bool result = false;
      llarp::Addr addr;
      {
        util::Lock l(m_LinksMutex);
        auto itr = m_Links.find(remote);
        if(itr == m_Links.end())
          return false;
        addr = itr->second;
      }
      {
        util::Lock l(m_SessionsMutex);
        auto itr = m_Sessions.find(addr);
        if(itr == m_Sessions.end())
          return false;
        result = itr->second->SendMessageBuffer(buf);
      }
      return result;
    }

    virtual bool
    GetOurAddressInfo(llarp::AddressInfo& addr) const = 0;

   protected:
    llarp_router* m_router;
    Addr m_ourAddr;
    llarp_udp_io m_udp;
    util::Mutex m_LinksMutex;
    util::Mutex m_SessionsMutex;
    std::unordered_map< PubKey, Addr, PubKey::Hash > m_Links;
    std::unordered_map< Addr, std::unique_ptr< ILinkSession >, Addr::Hash >
        m_Sessions;
  };
}  // namespace llarp

#endif
