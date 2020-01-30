#include <router/rc_gossiper.hpp>
#include <messages/dht_immediate.hpp>
#include <dht/messages/gotrouter.hpp>

namespace llarp
{
  RCGossiper::RCGossiper()
      : I_RCGossiper(), m_Filter(RouterContact::UpdateInterval)
  {
  }

  void
  RCGossiper::Init(ILinkManager* l)
  {
    m_LinkManager = l;
  }

  void
  RCGossiper::Decay(llarp_time_t now)
  {
    m_Filter.Decay(now);
  }

  bool
  RCGossiper::GossipRC(const RouterContact& rc)
  {
    // only distribute public routers
    if(not rc.IsPublicRouter())
      return false;
    if(m_LinkManager == nullptr)
      return false;
    // check for filter hit
    if(not m_Filter.Insert(rc.pubkey))
      return false;
    bool sent = false;
    // unwarrented GRCM
    DHTImmediateMessage m;
    m.msgs.emplace_back(
        new dht::GotRouterMessage(dht::Key_t{}, 0, {rc}, false));

    m_LinkManager->ForEachPeer([&](ILinkSession* s) {
      // ensure connected session
      if(not(s && s->IsEstablished()))
        return;
      // check if public router
      const auto other_rc = s->GetRemoteRC();
      if(not other_rc.IsPublicRouter())
        return;
      // dont send it to the owner
      if(other_rc.pubkey == rc.pubkey)
        return;
      // encode message
      ILinkSession::Message_t msg;
      msg.resize(1024);
      llarp_buffer_t buf(msg);
      if(not m.BEncode(&buf))
        return;
      msg.resize(buf.cur - buf.base);
      // send message
      if(s->SendMessageBuffer(std::move(msg), nullptr))
      {
        sent = true;
      }
    });
    return sent;
  }

}  // namespace llarp
