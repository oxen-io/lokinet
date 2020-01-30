#ifndef LLARP_RC_GOSSIPER_HPP
#define LLARP_RC_GOSSIPER_HPP

#include <util/decaying_hashset.hpp>
#include <router/i_gossiper.hpp>
#include <router/i_outbound_message_handler.hpp>
#include <link/i_link_manager.hpp>

namespace llarp
{
  struct RCGossiper : public I_RCGossiper
  {
    RCGossiper();

    ~RCGossiper() override = default;

    bool
    GossipRC(const RouterContact &rc) override;

    void
    Decay(llarp_time_t now) override;

    void
    Init(ILinkManager *);

   private:
    ILinkManager *m_LinkManager = nullptr;
    util::DecayingHashSet< RouterID > m_Filter;
  };
}  // namespace llarp

#endif
