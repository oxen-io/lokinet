#include <iwp/linklayer.hpp>
#include <iwp/session.hpp>
#include <config/key_manager.hpp>
#include <memory>
#include <unordered_set>

namespace llarp::iwp
{
  LinkLayer::LinkLayer(
      std::shared_ptr<KeyManager> keyManager,
      std::shared_ptr<EventLoop> ev,
      GetRCFunc getrc,
      LinkMessageHandler h,
      SignBufferFunc sign,
      BeforeConnectFunc_t before,
      SessionEstablishedHandler est,
      SessionRenegotiateHandler reneg,
      TimeoutHandler timeout,
      SessionClosedHandler closed,
      PumpDoneHandler pumpDone,
      WorkerFunc_t worker,
      bool allowInbound)
      : ILinkLayer(
          keyManager, getrc, h, sign, before, est, reneg, timeout, closed, pumpDone, worker)
      , m_Wakeup{ev->make_event_loop_waker([self = this]() { self->HandleWakeupPlaintext(); })}
      , m_PlaintextRecv{1024}
      , permitInbound{allowInbound}

  {}

  LinkLayer::~LinkLayer()
  {
    m_Wakeup->End();
  }

  const char*
  LinkLayer::Name() const
  {
    return "iwp";
  }

  uint16_t
  LinkLayer::Rank() const
  {
    return 2;
  }

  void
  LinkLayer::RecvFrom(const SockAddr& from, ILinkSession::Packet_t pkt)
  {
    std::shared_ptr<ILinkSession> session;
    auto itr = m_AuthedAddrs.find(from);
    bool isNewSession = false;
    if (itr == m_AuthedAddrs.end())
    {
      Lock_t lock(m_PendingMutex);
      if (m_Pending.count(from) == 0)
      {
        if (not permitInbound)
          return;
        isNewSession = true;
        m_Pending.insert({from, std::make_shared<Session>(this, from)});
      }
      session = m_Pending.find(from)->second;
    }
    else
    {
      Lock_t lock(m_AuthedLinksMutex);
      auto range = m_AuthedLinks.equal_range(itr->second);
      session = range.first->second;
    }
    if (session)
    {
      bool success = session->Recv_LL(std::move(pkt));
      if (!success and isNewSession)
      {
        LogWarn("Brand new session failed; removing from pending sessions list");
        m_Pending.erase(m_Pending.find(from));
      }
    }
  }

  bool
  LinkLayer::MapAddr(const RouterID& r, ILinkSession* s)
  {
    if (!ILinkLayer::MapAddr(r, s))
      return false;
    m_AuthedAddrs.emplace(s->GetRemoteEndpoint(), r);
    return true;
  }

  void
  LinkLayer::UnmapAddr(const SockAddr& addr)
  {
    m_AuthedAddrs.erase(addr);
  }

  std::shared_ptr<ILinkSession>
  LinkLayer::NewOutboundSession(const RouterContact& rc, const AddressInfo& ai)
  {
    return std::make_shared<Session>(this, rc, ai);
  }

  void
  LinkLayer::AddWakeup(std::weak_ptr<Session> session)
  {
    if (auto ptr = session.lock())
      m_PlaintextRecv[ptr->GetRemoteEndpoint()] = session;
  }

  void
  LinkLayer::WakeupPlaintext()
  {
    m_Wakeup->Wakeup();
  }

  void
  LinkLayer::HandleWakeupPlaintext()
  {
    for (const auto& [addr, session] : m_PlaintextRecv)
    {
      auto ptr = session.lock();
      if (ptr)
        ptr->HandlePlaintext();
    }
    m_PlaintextRecv.clear();
    PumpDone();
  }

}  // namespace llarp::iwp
