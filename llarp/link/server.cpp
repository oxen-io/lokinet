#include "server.hpp"
#include <llarp/ev/ev.hpp>
#include <llarp/ev/udp_handle.hpp>
#include <llarp/crypto/crypto.hpp>
#include <llarp/config/key_manager.hpp>
#include <memory>
#include <llarp/util/fs.hpp>
#include <utility>
#include <unordered_set>

static constexpr auto LINK_LAYER_TICK_INTERVAL = 100ms;

namespace llarp
{
  ILinkLayer::ILinkLayer(
      std::shared_ptr<KeyManager> keyManager,
      GetRCFunc getrc,
      LinkMessageHandler handler,
      SignBufferFunc signbuf,
      BeforeConnectFunc_t before,
      SessionEstablishedHandler establishedSession,
      SessionRenegotiateHandler reneg,
      TimeoutHandler timeout,
      SessionClosedHandler closed,
      PumpDoneHandler pumpDone,
      WorkerFunc_t work)
      : HandleMessage(std::move(handler))
      , HandleTimeout(std::move(timeout))
      , Sign(std::move(signbuf))
      , GetOurRC(std::move(getrc))
      , BeforeConnect(std::move(before))
      , SessionEstablished(std::move(establishedSession))
      , SessionClosed(std::move(closed))
      , SessionRenegotiate(std::move(reneg))
      , PumpDone(std::move(pumpDone))
      , QueueWork(std::move(work))
      , m_RouterEncSecret(keyManager->encryptionKey)
      , m_SecretKey(keyManager->transportKey)
  {}

  ILinkLayer::~ILinkLayer() = default;

  bool
  ILinkLayer::HasSessionTo(const RouterID& id)
  {
    Lock_t l(m_AuthedLinksMutex);
    return m_AuthedLinks.find(id) != m_AuthedLinks.end();
  }

  std::shared_ptr<ILinkSession>
  ILinkLayer::FindSessionByPubkey(RouterID id)
  {
    Lock_t l(m_AuthedLinksMutex);
    auto itr = m_AuthedLinks.find(id);
    if (itr == m_AuthedLinks.end())
      return nullptr;
    return itr->second;
  }

  void
  ILinkLayer::ForEachSession(std::function<void(const ILinkSession*)> visit, bool randomize) const
  {
    std::vector<std::shared_ptr<ILinkSession>> sessions;
    {
      Lock_t l(m_AuthedLinksMutex);
      if (m_AuthedLinks.size() == 0)
        return;
      const size_t sz = randint() % m_AuthedLinks.size();
      auto itr = m_AuthedLinks.begin();
      auto begin = itr;
      if (randomize)
      {
        std::advance(itr, sz);
        begin = itr;
      }
      while (itr != m_AuthedLinks.end())
      {
        sessions.emplace_back(itr->second);
        ++itr;
      }
      if (randomize)
      {
        itr = m_AuthedLinks.begin();
        while (itr != begin)
        {
          sessions.emplace_back(itr->second);
          ++itr;
        }
      }
    }
    for (const auto& session : sessions)
      visit(session.get());
  }

  bool
  ILinkLayer::VisitSessionByPubkey(const RouterID& pk, std::function<bool(ILinkSession*)> visit)
  {
    std::shared_ptr<ILinkSession> session;
    {
      Lock_t l(m_AuthedLinksMutex);
      auto itr = m_AuthedLinks.find(pk);
      if (itr == m_AuthedLinks.end())
        return false;
      session = itr->second;
    }
    return visit(session.get());
  }

  void
  ILinkLayer::ForEachSession(std::function<void(ILinkSession*)> visit)
  {
    std::vector<std::shared_ptr<ILinkSession>> sessions;
    {
      Lock_t l(m_AuthedLinksMutex);
      auto itr = m_AuthedLinks.begin();
      while (itr != m_AuthedLinks.end())
      {
        sessions.emplace_back(itr->second);
        ++itr;
      }
    }
    for (const auto& s : sessions)
      visit(s.get());
  }

  bool
  ILinkLayer::Configure(EventLoop_ptr loop, const std::string& ifname, int af, uint16_t port)
  {
    m_Loop = std::move(loop);
    m_udp = m_Loop->make_udp(
        [this]([[maybe_unused]] UDPHandle& udp, const SockAddr& from, llarp_buffer_t buf) {
          ILinkSession::Packet_t pkt;
          pkt.resize(buf.sz);
          std::copy_n(buf.base, buf.sz, pkt.data());
          RecvFrom(from, std::move(pkt));
        });

    if (ifname == "*")
    {
      if (!AllInterfaces(af, m_ourAddr))
        return false;
    }
    else
    {
      if (const auto maybe = GetInterfaceAddr(ifname, af))
      {
        m_ourAddr = *maybe;
      }
      else
      {
        try
        {
          m_ourAddr = SockAddr{ifname + ":0"};
        }
        catch (const std::exception& e)
        {
          LogError(stringify("Could not use ifname ", ifname, " to configure ILinkLayer"));
          throw e;
        }
      }
    }
    m_ourAddr.setPort(port);
    if (not m_udp->listen(m_ourAddr))
      return false;

    m_Loop->add_ticker([this] { Pump(); });
    return true;
  }

  void
  ILinkLayer::Pump()
  {
    std::unordered_set<RouterID> closedSessions;
    std::vector<std::shared_ptr<ILinkSession>> closedPending;
    auto _now = Now();
    {
      Lock_t l(m_AuthedLinksMutex);
      auto itr = m_AuthedLinks.begin();
      while (itr != m_AuthedLinks.end())
      {
        if (not itr->second->TimedOut(_now))
        {
          itr->second->Pump();
          ++itr;
        }
        else
        {
          llarp::LogInfo("session to ", RouterID(itr->second->GetPubKey()), " timed out");
          itr->second->Close();
          closedSessions.emplace(itr->first);
          itr = m_AuthedLinks.erase(itr);
        }
      }
    }
    {
      Lock_t l(m_PendingMutex);

      auto itr = m_Pending.begin();
      while (itr != m_Pending.end())
      {
        if (not itr->second->TimedOut(_now))
        {
          itr->second->Pump();
          ++itr;
        }
        else
        {
          LogInfo("pending session at ", itr->first, " timed out");
          // defer call so we can acquire mutexes later
          closedPending.emplace_back(std::move(itr->second));
          itr = m_Pending.erase(itr);
        }
      }
    }
    {
      Lock_t l(m_AuthedLinksMutex);
      for (const auto& r : closedSessions)
      {
        if (m_AuthedLinks.count(r) == 0)
        {
          SessionClosed(r);
        }
      }
    }
    for (const auto& pending : closedPending)
    {
      if (pending->IsInbound())
        continue;
      HandleTimeout(pending.get());
    }
  }

  bool
  ILinkLayer::MapAddr(const RouterID& pk, ILinkSession* s)
  {
    Lock_t l_authed(m_AuthedLinksMutex);
    Lock_t l_pending(m_PendingMutex);
    const auto addr = s->GetRemoteEndpoint();
    auto itr = m_Pending.find(addr);
    if (itr != m_Pending.end())
    {
      if (m_AuthedLinks.count(pk))
      {
        LogWarn("too many session for ", pk);
        s->Close();
        return false;
      }
      m_AuthedLinks.emplace(pk, itr->second);
      itr = m_Pending.erase(itr);
      return true;
    }
    return false;
  }

  bool
  ILinkLayer::PickAddress(const RouterContact& rc, llarp::AddressInfo& picked) const
  {
    std::string OurDialect = Name();
    for (const auto& addr : rc.addrs)
    {
      if (addr.dialect == OurDialect)
      {
        picked = addr;
        return true;
      }
    }
    return false;
  }

  util::StatusObject
  ILinkLayer::ExtractStatus() const
  {
    std::vector<util::StatusObject> pending, established;

    {
      Lock_t l(m_PendingMutex);
      std::transform(
          m_Pending.cbegin(),
          m_Pending.cend(),
          std::back_inserter(pending),
          [](const auto& item) -> util::StatusObject { return item.second->ExtractStatus(); });
    }
    {
      Lock_t l(m_AuthedLinksMutex);
      std::transform(
          m_AuthedLinks.cbegin(),
          m_AuthedLinks.cend(),
          std::back_inserter(established),
          [](const auto& item) -> util::StatusObject { return item.second->ExtractStatus(); });
    }

    return {
        {"name", Name()},
        {"rank", uint64_t(Rank())},
        {"addr", m_ourAddr.toString()},
        {"sessions", util::StatusObject{{"pending", pending}, {"established", established}}}};
  }

  bool
  ILinkLayer::TryEstablishTo(RouterContact rc)
  {
    {
      Lock_t l(m_AuthedLinksMutex);
      if (m_AuthedLinks.count(rc.pubkey))
      {
        LogWarn("Too many links to ", RouterID{rc.pubkey}, ", not establishing another one");
        return false;
      }
    }
    llarp::AddressInfo to;
    if (not PickAddress(rc, to))
    {
      LogWarn("router ", RouterID{rc.pubkey}, " has no acceptable inbound addresses");
      return false;
    }
    const SockAddr address{to};
    {
      Lock_t l(m_PendingMutex);
      if (m_Pending.count(address))
      {
        LogWarn(
            "Too many pending connections to ",
            address,
            " while establishing to ",
            RouterID{rc.pubkey},
            ", not establishing another");
        return false;
      }
    }
    std::shared_ptr<ILinkSession> s = NewOutboundSession(rc, to);
    if (BeforeConnect)
    {
      BeforeConnect(std::move(rc));
    }
    if (not PutSession(s))
    {
      return false;
    }
    s->Start();
    return true;
  }

  bool
  ILinkLayer::Start()
  {
    // Tie the lifetime of this repeater to this arbitrary shared_ptr:
    m_repeater_keepalive = std::make_shared<int>(0);
    m_Loop->call_every(LINK_LAYER_TICK_INTERVAL, m_repeater_keepalive, [this] { Tick(Now()); });
    return true;
  }

  void
  ILinkLayer::Tick(const llarp_time_t now)
  {
    {
      Lock_t l(m_AuthedLinksMutex);
      for (const auto& [routerid, link] : m_AuthedLinks)
        link->Tick(now);
    }

    {
      Lock_t l(m_PendingMutex);
      for (const auto& [addr, link] : m_Pending)
        link->Tick(now);
    }

    {
      // decay recently closed list
      auto itr = m_RecentlyClosed.begin();
      while (itr != m_RecentlyClosed.end())
      {
        if (itr->second >= now)
          itr = m_RecentlyClosed.erase(itr);
        else
          ++itr;
      }
    }
  }

  void
  ILinkLayer::Stop()
  {
    m_repeater_keepalive.reset();  // make the repeater kill itself
    {
      Lock_t l(m_AuthedLinksMutex);
      for (const auto& [router, link] : m_AuthedLinks)
        link->Close();
    }
    {
      Lock_t l(m_PendingMutex);
      for (const auto& [addr, link] : m_Pending)
        link->Close();
    }
  }

  void
  ILinkLayer::CloseSessionTo(const RouterID& remote)
  {
    static constexpr auto CloseGraceWindow = 500ms;
    const auto now = Now();
    {
      Lock_t l(m_AuthedLinksMutex);
      RouterID r = remote;
      llarp::LogInfo("Closing all to ", r);
      auto [itr, end] = m_AuthedLinks.equal_range(r);
      while (itr != end)
      {
        itr->second->Close();
        m_RecentlyClosed.emplace(itr->second->GetRemoteEndpoint(), now + CloseGraceWindow);
        itr = m_AuthedLinks.erase(itr);
      }
    }
    SessionClosed(remote);
  }

  void
  ILinkLayer::KeepAliveSessionTo(const RouterID& remote)
  {
    Lock_t l(m_AuthedLinksMutex);
    for (auto [itr, end] = m_AuthedLinks.equal_range(remote); itr != end; ++itr)
    {
      if (itr->second->ShouldPing())
      {
        LogDebug("keepalive to ", remote);
        itr->second->SendKeepAlive();
      }
    }
  }

  void
  ILinkLayer::SendTo_LL(const SockAddr& to, const llarp_buffer_t& pkt)
  {
    m_udp->send(to, pkt);
  }

  bool
  ILinkLayer::SendTo(
      const RouterID& remote, const llarp_buffer_t& buf, ILinkSession::CompletionHandler completed)
  {
    std::shared_ptr<ILinkSession> s;
    {
      Lock_t l(m_AuthedLinksMutex);
      // pick lowest backlog session
      size_t min = std::numeric_limits<size_t>::max();

      for (auto [itr, end] = m_AuthedLinks.equal_range(remote); itr != end; ++itr)
      {
        if (const auto backlog = itr->second->SendQueueBacklog(); backlog < min)
        {
          s = itr->second;
          min = backlog;
        }
      }
    }
    ILinkSession::Message_t pkt(buf.sz);
    std::copy_n(buf.base, buf.sz, pkt.begin());
    return s && s->SendMessageBuffer(std::move(pkt), completed);
  }

  bool
  ILinkLayer::GetOurAddressInfo(llarp::AddressInfo& addr) const
  {
    addr.fromSockAddr(m_ourAddr);
    addr.dialect = Name();
    addr.pubkey = TransportPubKey();
    addr.rank = Rank();
    return true;
  }

  const byte_t*
  ILinkLayer::TransportPubKey() const
  {
    return llarp::seckey_topublic(TransportSecretKey());
  }

  const SecretKey&
  ILinkLayer::TransportSecretKey() const
  {
    return m_SecretKey;
  }

  bool
  ILinkLayer::PutSession(const std::shared_ptr<ILinkSession>& s)
  {
    Lock_t lock(m_PendingMutex);
    const auto address = s->GetRemoteEndpoint();
    if (m_Pending.count(address))
      return false;
    m_Pending.emplace(address, s);
    return true;
  }

  std::optional<int>
  ILinkLayer::GetUDPFD() const
  {
    return m_udp->file_descriptor();
  }

}  // namespace llarp
