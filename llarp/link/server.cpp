#include <fs.hpp>
#include <link/server.hpp>

namespace llarp
{
  ILinkLayer::ILinkLayer(const byte_t* routerEncSecret, GetRCFunc getrc,
                         LinkMessageHandler handler, SignBufferFunc signbuf,
                         SessionEstablishedHandler establishedSession,
                         SessionRenegotiateHandler reneg,
                         TimeoutHandler timeout, SessionClosedHandler closed)
      : HandleMessage(handler)
      , HandleTimeout(timeout)
      , Sign(signbuf)
      , GetOurRC(getrc)
      , SessionEstablished(establishedSession)
      , SessionClosed(closed)
      , SessionRenegotiate(reneg)
      , m_RouterEncSecret(routerEncSecret)
  {
  }

  ILinkLayer::~ILinkLayer()
  {
  }

  bool
  ILinkLayer::HasSessionTo(const byte_t* pk)
  {
    Lock l(m_AuthedLinksMutex);
    return m_AuthedLinks.find(pk) != m_AuthedLinks.end();
  }

  void
  ILinkLayer::ForEachSession(
      std::function< void(const ILinkSession*) > visit) const
  {
    auto itr = m_AuthedLinks.begin();
    while(itr != m_AuthedLinks.end())
    {
      visit(itr->second.get());
      ++itr;
    }
  }

  bool
  ILinkLayer::VisitSessionByPubkey(const byte_t* pk,
                                   std::function< bool(ILinkSession*) > visit)
  {
    auto itr = m_AuthedLinks.find(pk);
    if(itr != m_AuthedLinks.end())
    {
      return visit(itr->second.get());
    }
    return false;
  }

  void
  ILinkLayer::ForEachSession(std::function< void(ILinkSession*) > visit)
  {
    auto itr = m_AuthedLinks.begin();
    while(itr != m_AuthedLinks.end())
    {
      visit(itr->second.get());
      ++itr;
    }
  }

  bool
  ILinkLayer::Configure(llarp_ev_loop* loop, const std::string& ifname, int af,
                        uint16_t port)
  {
    m_Loop         = loop;
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
    auto _now = Now();
    {
      Lock lock(m_AuthedLinksMutex);
      auto itr = m_AuthedLinks.begin();
      while(itr != m_AuthedLinks.end())
      {
        if(!itr->second->TimedOut(_now))
        {
          itr->second->Pump();
          ++itr;
        }
        else
        {
          llarp::LogInfo("session to ", RouterID(itr->second->GetPubKey()),
                         " timed out");
          itr = m_AuthedLinks.erase(itr);
        }
      }
    }
    {
      Lock lock(m_PendingMutex);

      auto itr = m_Pending.begin();
      while(itr != m_Pending.end())
      {
        if(!(*itr)->TimedOut(_now))
        {
          (*itr)->Pump();
          ++itr;
        }
        else
          itr = m_Pending.erase(itr);
      }
    }
  }

  void
  ILinkLayer::MapAddr(const byte_t* pk, ILinkSession* s)
  {
    static constexpr size_t MaxSessionsPerKey = 16;
    Lock l_authed(m_AuthedLinksMutex);
    Lock l_pending(m_PendingMutex);
    auto itr = m_Pending.begin();
    while(itr != m_Pending.end())
    {
      if(itr->get() == s)
      {
        if(m_AuthedLinks.count(pk) < MaxSessionsPerKey)
          m_AuthedLinks.insert(std::make_pair(pk, std::move(*itr)));
        else
          s->SendClose();
        itr = m_Pending.erase(itr);
        return;
      }
      else
        ++itr;
    }
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

  bool
  ILinkLayer::TryEstablishTo(const RouterContact& rc)
  {
    llarp::AddressInfo to;
    if(!PickAddress(rc, to))
      return false;
    llarp::LogInfo("Try establish to ", RouterID(rc.pubkey.data()));
    llarp::Addr addr(to);
    auto s = NewOutboundSession(rc, to);
    s->Start();
    PutSession(s);
    return true;
  }

  bool
  ILinkLayer::Start(Logic* l)
  {
    m_Logic = l;
    ScheduleTick(100);
    return true;
  }

  void
  ILinkLayer::Stop()
  {
    if(m_Logic && tick_id)
      m_Logic->remove_call(tick_id);
    {
      Lock l(m_AuthedLinksMutex);
      auto itr = m_AuthedLinks.begin();
      while(itr != m_AuthedLinks.end())
      {
        itr->second->SendClose();
        itr = m_AuthedLinks.erase(itr);
      }
    }
    {
      Lock l(m_PendingMutex);
      auto itr = m_Pending.begin();
      while(itr != m_Pending.end())
      {
        (*itr)->SendClose();
        itr = m_Pending.erase(itr);
      }
    }
  }

  void
  ILinkLayer::CloseSessionTo(const byte_t* remote)
  {
    Lock l(m_AuthedLinksMutex);
    RouterID r = remote;
    llarp::LogInfo("Closing all to ", r);
    auto range = m_AuthedLinks.equal_range(r);
    auto itr   = range.first;
    while(itr != range.second)
    {
      itr->second->SendClose();
      itr = m_AuthedLinks.erase(itr);
    }
  }

  void
  ILinkLayer::KeepAliveSessionTo(const byte_t* remote)
  {
    Lock l(m_AuthedLinksMutex);
    auto range = m_AuthedLinks.equal_range(remote);
    auto itr   = range.first;
    while(itr != range.second)
    {
      itr->second->SendKeepAlive();
      ++itr;
    }
  }

  bool
  ILinkLayer::SendTo(const byte_t* remote, llarp_buffer_t buf)
  {
    ILinkSession* s = nullptr;
    {
      Lock l(m_AuthedLinksMutex);
      auto range = m_AuthedLinks.equal_range(remote);
      auto itr   = range.first;
      // pick lowest backlog session
      size_t min = std::numeric_limits< size_t >::max();

      while(itr != range.second)
      {
        auto backlog = itr->second->SendQueueBacklog();
        if(backlog < min)
        {
          s   = itr->second.get();
          min = backlog;
        }
        ++itr;
      }
    }
    return s && s->SendMessageBuffer(buf);
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
  ILinkLayer::GenEphemeralKeys()
  {
    return KeyGen(m_SecretKey);
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
  ILinkLayer::PutSession(ILinkSession* s)
  {
    Lock lock(m_PendingMutex);
    m_Pending.emplace_back(s);
  }

  void
  ILinkLayer::OnTick(uint64_t interval)
  {
    Tick(Now());
    ScheduleTick(interval);
  }

  void
  ILinkLayer::ScheduleTick(uint64_t interval)
  {
    tick_id = m_Logic->call_later({interval, this, &ILinkLayer::on_timer_tick});
  }

}  // namespace llarp
