#include "linklayer.hpp"
#include "llarp/link/server.hpp"
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
      , m_Inbound{allowInbound}
  {}

  std::string_view
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
      auto it = m_Pending.find(from);
      if (it == m_Pending.end())
      {
        if (not m_Inbound)
          return;
        isNewSession = true;
        it = m_Pending.emplace(from, std::make_shared<Session>(this, from)).first;
      }
      session = it->second;
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
        LogDebug("Brand new session failed; removing from pending sessions list");
        m_Pending.erase(from);
      }
      WakeupPlaintext();
    }
  }

  std::shared_ptr<ILinkSession>
  LinkLayer::NewOutboundSession(const RouterContact& rc, const AddressInfo& ai)
  {
    if (m_Inbound)
      throw std::logic_error{"inbound link cannot make outbound sessions"};
    return std::make_shared<Session>(this, rc, ai);
  }

  void
  LinkLayer::WakeupPlaintext()
  {
    m_Wakeup->Trigger();
  }

  void
  LinkLayer::HandleWakeupPlaintext()
  {
    // Copy bare pointers out first because HandlePlaintext can end up removing themselves from the
    // structures.
    m_WakingUp.clear();  // Reused to minimize allocations.
    for (const auto& [router_id, session] : m_AuthedLinks)
      m_WakingUp.push_back(session.get());
    for (const auto& [addr, session] : m_Pending)
      m_WakingUp.push_back(session.get());
    for (auto* session : m_WakingUp)
      session->HandlePlaintext();
    PumpDone();
  }

}  // namespace llarp::iwp
