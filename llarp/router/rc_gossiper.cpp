#include "rc_gossiper.hpp"
#include <llarp/messages/dht_immediate.hpp>
#include <llarp/dht/messages/gotrouter.hpp>
#include <llarp/util/time.hpp>
#include <llarp/constants/link_layer.hpp>
#include <llarp/tooling/rc_event.hpp>

namespace llarp
{
  // 30 minutes
  static constexpr auto RCGossipFilterDecayInterval = 30min;
  // (30 minutes * 2) - 5 minutes
  static constexpr auto GossipOurRCInterval = (RCGossipFilterDecayInterval * 2) - (5min);

  RCGossiper::RCGossiper()
      : I_RCGossiper(), m_Filter(std::chrono::duration_cast<Time_t>(RCGossipFilterDecayInterval))
  {}

  void
  RCGossiper::Init(ILinkManager* l, const RouterID& ourID, AbstractRouter* router)
  {
    m_OurRouterID = ourID;
    m_LinkManager = l;
    m_router = router;
  }

  bool
  RCGossiper::ShouldGossipOurRC(Time_t now) const
  {
    bool should = now >= (m_LastGossipedOurRC + GossipOurRCInterval);
    LogWarn("ShouldGossipOurRC: ", should);
    return should;
  }

  bool
  RCGossiper::IsOurRC(const RouterContact& rc) const
  {
    return rc.pubkey == m_OurRouterID;
  }

  void
  RCGossiper::Decay(Time_t now)
  {
    m_Filter.Decay(now);
  }

  bool
  RCGossiper::GossipRC(const RouterContact& rc)
  {
    // only distribute public routers
    if (not rc.IsPublicRouter())
      return false;
    if (m_LinkManager == nullptr)
      return false;
    const RouterID pubkey(rc.pubkey);
    // filter check
    if (m_Filter.Contains(pubkey))
      return false;
    m_Filter.Insert(pubkey);

    const auto now = time_now_ms();
    // is this our rc?
    if (IsOurRC(rc))
    {
      // should we gossip our rc?
      if (not ShouldGossipOurRC(now))
      {
        // nah drop it
        return false;
      }
      // ya pop it
      m_LastGossipedOurRC = now;
    }

    // send a GRCM as gossip method
    DHTImmediateMessage gossip;
    gossip.msgs.emplace_back(new dht::GotRouterMessage(dht::Key_t{}, 0, {rc}, false));

    // send it to everyone
    m_LinkManager->ForEachPeer([&](ILinkSession* peerSession) {
      // ensure connected session
      if (not(peerSession && peerSession->IsEstablished()))
        return;
      // check if public router
      const auto other_rc = peerSession->GetRemoteRC();
      if (not other_rc.IsPublicRouter())
        return;
      // encode message
      ILinkSession::Message_t msg;
      msg.resize(MAX_LINK_MSG_SIZE / 2);
      llarp_buffer_t buf(msg);
      if (not gossip.BEncode(&buf))
        return;
      msg.resize(buf.cur - buf.base);

      m_router->NotifyRouterEvent<tooling::RCGossipSentEvent>(m_router->pubkey(), rc);

      // send message
      peerSession->SendMessageBuffer(std::move(msg), nullptr);
    });
    return true;
  }

}  // namespace llarp
