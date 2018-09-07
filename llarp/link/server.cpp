#include <llarp/link/server.hpp>
#include "fs.hpp"

namespace llarp
{
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
    m_ourAddr.port(port);
    return llarp_ev_add_udp(loop, &m_udp, m_ourAddr) != -1;
  }

  void
  ILinkLayer::Pump()
  {
    util::Lock lock(m_SessionsMutex);
    auto now = llarp_time_now_ms();
    auto itr = m_Sessions.begin();
    while(itr != m_Sessions.end())
    {
      if(itr->second->TimedOut(now))
      {
        util::Lock lock(m_LinksMutex);
        auto i = m_Links.find(itr->second->GetPubKey());
        if(i != m_Links.end())
          m_Links.erase(i);
        itr = m_Sessions.erase(itr);
      }
      else
      {
        itr->second->Pump();
        ++itr;
      }
    }
  }

  void
  ILinkLayer::MapAddr(const PubKey& pk, ILinkSession* s)
  {
    util::Lock l(m_LinksMutex);
    auto itr = m_Links.find(pk);
    // delete old session
    if(itr != m_Links.end())
    {
      llarp::LogDebug("close previously authed session to ", pk);
      itr->second->SendClose();
      m_Links.erase(itr);
    }
    // insert new session
    m_Links.insert(std::make_pair(pk, s));
  }

  bool
  ILinkLayer::PickAddress(const RouterContact& rc,
                          llarp::AddressInfo& picked) const
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
    llarp::AddressInfo to;
    if(!PickAddress(rc, to))
      return;
    llarp::Addr addr(to);
    auto s = NewOutboundSession(rc, to);
    s->Start();
    PutSession(addr, s);
  }

  bool
  ILinkLayer::Start(llarp_logic* l)
  {
    m_Logic = l;
    ScheduleTick(100);
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
    util::Lock l(m_LinksMutex);
    auto itr = m_Links.find(remote);
    if(itr == m_Links.end())
      return;
    itr->second->SendClose();
    m_Links.erase(itr);
  }

  void
  ILinkLayer::KeepAliveSessionTo(const PubKey& remote)
  {
    util::Lock l(m_LinksMutex);
    auto itr = m_Links.find(remote);
    if(itr == m_Links.end())
      return;
    itr->second->SendKeepAlive();
  }

  bool
  ILinkLayer::SendTo(const PubKey& remote, llarp_buffer_t buf)
  {
    util::Lock l(m_LinksMutex);
    auto itr = m_Links.find(remote);
    if(itr == m_Links.end())
      return false;
    return itr->second->SendMessageBuffer(buf);
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
    return llarp::seckey_topublic(TransportSecretKey());
  }

  const byte_t*
  ILinkLayer::TransportSecretKey() const
  {
    return m_SecretKey;
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
    {
      llarp::LogError("Failed to load keyfile ", f);
      return false;
    }
    return true;
  }

  void
  ILinkLayer::PutSession(const Addr& addr, ILinkSession* s)
  {
    util::Lock lock(m_SessionsMutex);
    m_Sessions.insert(std::make_pair(addr, s));
  }

  void
  ILinkLayer::OnTick(uint64_t interval, llarp_time_t now)
  {
    Tick(now);
    util::Lock l(m_SessionsMutex);
    auto itr = m_Sessions.begin();
    while(itr != m_Sessions.end())
    {
      itr->second->Tick(now);
      ++itr;
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
