#include <llarp/link/server.hpp>
#include "fs.hpp"

namespace llarp
{
  ILinkLayer::ILinkLayer(llarp_router* r) : m_router(r)
  {
  }

  ILinkLayer::~ILinkLayer()
  {
  }

  bool
  ILinkLayer::HasSessionTo(const PubKey& pk)
  {
    util::Lock l(m_LinksMutex);
    return m_Links.find(pk) != m_Links.end();
  }

  bool
  ILinkLayer::HasSessionVia(const Addr& addr)
  {
    util::Lock l(m_SessionsMutex);
    return m_Sessions.find(addr) != m_Sessions.end();
  }

  bool
  ILinkLayer::Configure(llarp_ev_loop* loop, const std::string& ifname, int af,
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

  void
  ILinkLayer::Pump()
  {
    auto now = llarp_time_now_ms();
    util::Lock l(m_SessionsMutex);
    auto itr = m_Sessions.begin();
    while(itr != m_Sessions.end())
    {
      if(!itr->second->TimedOut(now))
      {
        itr->second->Pump();
        ++itr;
      }
      else
        itr = m_Sessions.erase(itr);
    }
  }

  void
  ILinkLayer::RecvFrom(const Addr& from, const void* buf, size_t sz)
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

  bool
  ILinkLayer::PickAddress(const RouterContact& rc, llarp::Addr& picked) const
  {
    std::string OurDialect = Name();
    for(const auto& addr : rc.addrs)
    {
      if(addr.dialect == OurDialect)
      {
        picked = addr;
        return true;
      }
    }
    return false;
  }

  void
  ILinkLayer::TryEstablishTo(const RouterContact& rc)
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

  bool
  ILinkLayer::Start(llarp_logic* l)
  {
    m_Logic = l;
    ScheduleTick(500);
    return true;
  }

  void
  ILinkLayer::Stop()
  {
    if(m_Logic && tick_id)
      llarp_logic_remove_call(m_Logic, tick_id);
  }

  void
  ILinkLayer::CloseSessionTo(const PubKey& remote)
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
  ILinkLayer::KeepAliveSessionTo(const PubKey& remote)
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
  ILinkLayer::SendTo(const PubKey& remote, llarp_buffer_t buf)
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

  bool
  ILinkLayer::GetOurAddressInfo(llarp::AddressInfo& addr) const
  {
    addr.dialect = Name();
    addr.pubkey  = TransportPubKey();
    addr.rank    = Rank();
    addr.port    = m_ourAddr.port();
    addr.ip      = *m_ourAddr.addr6();
    return true;
  }

  const byte_t*
  ILinkLayer::TransportPubKey() const
  {
    return llarp::seckey_topublic(m_SecretKey);
  }

  bool
  ILinkLayer::EnsureKeys(const char* f)
  {
    fs::path fpath(f);
    llarp::SecretKey keys;
    std::error_code ec;
    if(!fs::exists(fpath, ec))
    {
      if(!KeyGen(m_SecretKey))
        return false;
      // generated new keys
      if(!BEncodeWriteFile< decltype(keys), 128 >(f, m_SecretKey))
        return false;
    }
    // load keys
    if(!BDecodeReadFile(f, m_SecretKey))
      return false;
    return true;
  }

  void
  ILinkLayer::Tick(uint64_t interval, llarp_time_t now)
  {
    util::Lock l(m_SessionsMutex);
    auto itr = m_Sessions.begin();
    while(itr != m_Sessions.end())
    {
      if(!itr->second->TimedOut(now))
      {
        itr->second->Tick(now);
        ++itr;
      }
      else
        itr = m_Sessions.erase(itr);
    }
    ScheduleTick(interval);
  }

  void
  ILinkLayer::ScheduleTick(uint64_t interval)
  {
    tick_id = llarp_logic_call_later(
        m_Logic, {interval, this, &ILinkLayer::on_timer_tick});
  }

}  // namespace llarp
