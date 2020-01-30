#include <router/rc_gossiper.hpp>
#include <messages/dht_immediate.hpp>
#include <dht/messages/gotrouter.hpp>
#include <util/time.hpp>

namespace llarp
{
  // 30 minutes
  static constexpr auto RCGossipFilterDecayInterval = 30min;
  // 60 - 5 minutes
  static constexpr auto GossipOurRCInterval =
      (RCGossipFilterDecayInterval * 2) - (5min);

  RCGossiper::RCGossiper()
      : I_RCGossiper()
      , m_Filter(
            std::chrono::duration_cast< Time_t >(RCGossipFilterDecayInterval))
  {
  }

  void
  RCGossiper::Init(ILinkManager* l, const RouterID& ourID)
  {
    m_OurRouterID = ourID;
    m_LinkManager = l;
  }

  bool
  RCGossiper::ShouldGossipOurRC(Time_t now) const
  {
    return now >= (m_LastGossipedOurRC + GossipOurRCInterval);
  }

  bool
  RCGossiper::IsOurRC(const RouterContact& rc) const
  {
    return rc.pubkey == m_OurRouterID;
  }

  void
  RCGossiper::Decay(Time_t now)
  {
    LogDebug("decay filter at ", now.count());
    m_Filter.Decay(now.count());
  }

  bool
  RCGossiper::GossipRC(const RouterContact& rc)
  {
    // only distribute public routers
    if(not rc.IsPublicRouter())
      return false;
    if(m_LinkManager == nullptr)
      return false;
    const RouterID k(rc.pubkey);
    // filter check
    if(m_Filter.Contains(k))
      return false;
    m_Filter.Insert(k);

    const auto now = time_now();
    // is this our rc?
    if(IsOurRC(rc))
    {
      // should we gossip our rc?
      if(not ShouldGossipOurRC(now))
      {
        // nah drop it
        return false;
      }
      // ya pop it
      m_LastGossipedOurRC = now;
    }

    // send unwarrented GRCM as gossip method
    DHTImmediateMessage m;
    m.msgs.emplace_back(
        new dht::GotRouterMessage(dht::Key_t{}, 0, {rc}, false));
    // send it to everyone
    m_LinkManager->ForEachPeer([&](ILinkSession* s) {
      // ensure connected session
      if(not(s && s->IsEstablished()))
        return;
      // check if public router
      const auto other_rc = s->GetRemoteRC();
      if(not other_rc.IsPublicRouter())
        return;
      // encode message
      ILinkSession::Message_t msg;
      msg.reserve(1024);
      llarp_buffer_t buf(msg);
      if(not m.BEncode(&buf))
        return;
      msg.resize(buf.cur - buf.base);
      // send message
      s->SendMessageBuffer(std::move(msg), nullptr);
    });
    return true;
  }

}  // namespace llarp
