#pragma once

#include <llarp/path/transit_hop.hpp>
#include <llarp/crypto/types.hpp>
#include <memory>
#include "destination.hpp"

namespace llarp
{
  struct AbstractRouter;
}

namespace llarp::layers::route
{

  class RouteLayer
  {
    std::vector<std::unique_ptr<Destination>> _destinations;

   public:
    explicit RouteLayer(AbstractRouter&);

    /// create a routing destination belonging to pubkey+endpoint transithop going to a snode.
    Destination*
    create_destination(const path::TransitHopInfo& info, PubKey src, RouterID dst);

    /// remove a routing destination for an existing endpoint transit hop.
    /// returns true if we removed something.
    bool
    remove_destination_on(const path::TransitHopInfo& info);

    /// remove all routing destination from a source to a destination.
    /// returns the number of entries we removed.
    size_t
    remove_destinations_to(const PubKey& src, const RouterID& dst);

    /// remove all routing destination from a source.
    /// returns the number of entries we removed.
    size_t
    remove_destinations_from(const PubKey& dst);

    /// get a routing destination given a local path rxID.
    Destination*
    destination_to(const PathID_t& rxid);

    /// get a routing destination given its local path txid.
    Destination*
    destination_from(const PathID_t& txid);

    /// get all routing destinations given the source pubkey going to a snode dst.
    std::vector<Destination*>
    all_destinations_from(const PubKey& src);

    /// maybe fetch a routing destination that was previously created for an endpoint transit hop.
    Destination*
    destination_on(const path::TransitHopInfo& info) const;
  };
}  // namespace llarp::layers::route
