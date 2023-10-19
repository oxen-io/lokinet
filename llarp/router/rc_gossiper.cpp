#include "rc_gossiper.hpp"

#include <llarp/router_contact.hpp>
#include <llarp/util/time.hpp>

namespace llarp
{
  // 30 minutes
  static constexpr auto RCGossipFilterDecayInterval = 30min;
  // (30 minutes * 2) - 5 minutes
  static constexpr auto GossipOurRCInterval = (RCGossipFilterDecayInterval * 2) - (5min);

  RCGossiper::RCGossiper() : filter(std::chrono::duration_cast<Time_t>(RCGossipFilterDecayInterval))
  {}

  void
  RCGossiper::Init(LinkManager* l, const RouterID& ourID, Router* r)
  {
    rid = ourID;
    link_manager = l;
    router = r;
  }

  bool
  RCGossiper::ShouldGossipOurRC(Time_t now) const
  {
    return now >= (last_rc_gossip + GossipOurRCInterval);
  }

  bool
  RCGossiper::IsOurRC(const RouterContact& rc) const
  {
    return rc.pubkey == rid;
  }

  void
  RCGossiper::Decay(Time_t now)
  {
    filter.Decay(now);
  }

  void
  RCGossiper::Forget(const RouterID& pk)
  {
    filter.Remove(pk);
    if (rid == pk)
      last_rc_gossip = 0s;
  }

  TimePoint_t
  RCGossiper::NextGossipAt() const
  {
    if (auto maybe = LastGossipAt())
      return *maybe + GossipOurRCInterval;
    return DateClock_t::now();
  }

  std::optional<TimePoint_t>
  RCGossiper::LastGossipAt() const
  {
    if (last_rc_gossip == 0s)
      return std::nullopt;
    return DateClock_t::time_point{last_rc_gossip};
  }

  bool
  RCGossiper::GossipRC(const RouterContact& rc)
  {
    // only distribute public routers
    if (not rc.IsPublicRouter())
      return false;
    if (link_manager == nullptr)
      return false;
    const RouterID pubkey(rc.pubkey);
    // filter check
    if (filter.Contains(pubkey))
      return false;
    filter.Insert(pubkey);

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
      last_rc_gossip = now;
    }

    // send a GRCM as gossip method
    // DHTImmediateMessage gossip;
    // gossip.msgs.emplace_back(new dht::GotRouterMessage(dht::Key_t{}, 0, {rc}, false));

    // std::vector<RouterID> gossipTo;

    /*
     * TODO: gossip RC via libquic
     *
    // select peers to gossip to
    m_LinkManager->ForEachPeer(
        [&](const AbstractLinkSession* peerSession, bool) {
          // ensure connected session
          if (not(peerSession && peerSession->IsEstablished()))
            return;
          // check if public router
          const auto other_rc = peerSession->GetRemoteRC();
          if (not other_rc.IsPublicRouter())
            return;
          gossipTo.emplace_back(other_rc.pubkey);
        },
        true);

    std::unordered_set<RouterID> keys;
    // grab the keys we want to use
    std::sample(
        gossipTo.begin(), gossipTo.end(), std::inserter(keys, keys.end()), MaxGossipPeers,
    llarp::csrng);

    m_LinkManager->ForEachPeer([&](AbstractLinkSession* peerSession) {
      if (not(peerSession && peerSession->IsEstablished()))
        return;

      // exclude from gossip as we have not selected to use it
      if (keys.count(peerSession->GetPubKey()) == 0)
        return;

      // encode message
      AbstractLinkSession::Message_t msg{};
      msg.resize(MAX_LINK_MSG_SIZE / 2);
      llarp_buffer_t buf(msg);
      if (not gossip.BEncode(&buf))
        return;
      msg.resize(buf.cur - buf.base);

      m_router->NotifyRouterEvent<tooling::RCGossipSentEvent>(m_router->pubkey(), rc);

      // send message
      peerSession->SendMessageBuffer(std::move(msg), nullptr, gossip.Priority());
    });
     *
     *
     */

    return true;
  }

}  // namespace llarp
