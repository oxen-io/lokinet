#pragma once

#include <llarp/util/decaying_hashset.hpp>
#include "i_gossiper.hpp"
#include "i_outbound_message_handler.hpp"
#include <llarp/link/i_link_manager.hpp>
#include "abstractrouter.hpp"

namespace llarp
{
  struct RCGossiper : public I_RCGossiper
  {
    RCGossiper();

    ~RCGossiper() override = default;

    bool
    GossipRC(const RouterContact& rc) override;

    void
    Decay(Time_t now) override;

    bool
    ShouldGossipOurRC(Time_t now) const override;

    bool
    IsOurRC(const RouterContact& rc) const override;

    void
    Init(ILinkManager*, const RouterID&, AbstractRouter*);

   private:
    RouterID m_OurRouterID;
    Time_t m_LastGossipedOurRC = 0s;
    ILinkManager* m_LinkManager = nullptr;
    util::DecayingHashSet<RouterID> m_Filter;

    AbstractRouter* m_router;
  };
}  // namespace llarp
