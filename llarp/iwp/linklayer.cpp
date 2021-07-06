#include "linklayer.hpp"
#include "session.hpp"
#include <llarp/config/key_manager.hpp>
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
      , m_Wakeup{ev->make_waker([this]() { HandleWakeupPlaintext(); })}
      , m_PlaintextRecv{1024}
      , m_Inbound{allowInbound}

  {}

  const char*
  LinkLayer::Name() const
  {
    return "iwp";
  }

  std::string
  LinkLayer::PrintableName() const
  {
    if (m_Inbound)
      return "inbound iwp link";
    else
      return "outbound iwp link";
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
      Lock_t lock{m_PendingMutex};
      if (m_Pending.count(from) == 0)
      {
        if (not m_Inbound)
          return;
        isNewSession = true;
        m_Pending.insert({from, std::make_shared<Session>(this, from)});
      }
      session = m_Pending.find(from)->second;
    }
    else
    {
      if (auto s_itr = m_AuthedLinks.find(itr->second); s_itr != m_AuthedLinks.end())
        session = s_itr->second;
    }
    if (session)
    {
      bool success = session->Recv_LL(std::move(pkt));
      if (not success and isNewSession)
      {
        LogWarn("Brand new session failed; removing from pending sessions list");
        m_Pending.erase(m_Pending.find(from));
      }
    }
  }

  bool
  LinkLayer::MapAddr(const RouterID& r, ILinkSession* s)
  {
    if (not ILinkLayer::MapAddr(r, s))
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
    if (m_Inbound)
      throw std::logic_error{"inbound link cannot make outbound sessions"};
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
    m_Wakeup->Trigger();
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
