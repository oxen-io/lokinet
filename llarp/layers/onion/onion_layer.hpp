#pragma once

#include <unordered_map>
#include <unordered_set>

#include <llarp/path/path_types.hpp>
#include "onion_stats.hpp"

namespace llarp
{
  struct AbstractRouter;
  class EventLoopWakeup;
}  // namespace llarp

namespace llarp::path
{
  struct IHopHandler;
}

namespace llarp::layers::onion
{
  class OnionLayer
  {
    /// all paths that exist in our lokinet.
    std::unordered_set<std::shared_ptr<path::IHopHandler>> _all;
    /// map from pathid to owned path.
    std::unordered_map<PathID_t, std::weak_ptr<path::IHopHandler>> _id_to_owned;
    /// map from pathid to transit path.
    std::unordered_map<PathID_t, std::weak_ptr<path::IHopHandler>> _id_to_transit;

    AbstractRouter& _router;

    std::shared_ptr<EventLoopWakeup> _work;

    /// submit all work items for onion layer cryptography
    void
    submit_work() const;

    /// remove expired entires.
    void
    remove_expired();

   public:
    explicit OnionLayer(AbstractRouter&);

    /// idempotently tell the onion layer that we have work to submit.
    void
    trigger_work() const;

    void
    start();

    void
    stop();

    OnionStats
    current_stats() const;
  };

}  // namespace llarp::layers::onion
