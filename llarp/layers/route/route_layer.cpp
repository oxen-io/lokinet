#include "route_layer.hpp"

namespace llarp::layers::route
{
  RouteLayer::RouteLayer(AbstractRouter&)
  {}

  Destination*
  RouteLayer::create_destination(const path::TransitHopInfo&, PubKey, RouterID)
  {
    // todo: implement me.
    return nullptr;
  }

  bool
  RouteLayer::remove_destination_on(const path::TransitHopInfo&)
  {
    // todo: implement me.
    return false;
  }

  size_t
  RouteLayer::remove_destinations_from(const PubKey&)
  {
    // todo: implement me.
    return 0;
  }

  size_t
  RouteLayer::remove_destinations_to(const PubKey&, const RouterID&)
  {
    // todo: implement me.
    return 0;
  }

  Destination*
  RouteLayer::destination_to(const PathID_t&)
  {
    // todo: implement me.
    return nullptr;
  }

  Destination*
  RouteLayer::destination_from(const PathID_t&)
  {
    // todo: implement me.
    return nullptr;
  }

  std::vector<Destination*>
  RouteLayer::all_destinations_from(const PubKey&)
  {
    // todo: implement me.
    return {};
  }

  Destination*
  RouteLayer::destination_on(const path::TransitHopInfo&) const
  {
    // todo: implement me.
    return nullptr;
  }
}  // namespace llarp::layers::route
