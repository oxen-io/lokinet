#pragma once

#include <optional>
#include <unordered_set>
#include <llarp/router_id.hpp>

namespace llarp
{
  struct AbstractRouter;
  struct RouterContact;
}  // namespace llarp

namespace llarp::consensus
{
  /// when we want to connect to and edge when we are a client, an instance of EdgeSelector acts as
  /// a stateless selection algorith for router contacts for each attempt at connecting out we make.
  class EdgeSelector
  {
    AbstractRouter& _router;

   public:
    explicit EdgeSelector(AbstractRouter& router);

    /// select a candidate for connecting out to the network when we need more connections to do a
    /// path build. pass in an unordered set of the snode pubkeys we are currently connected to.
    std::optional<RouterContact>
    select_path_edge(const std::unordered_set<RouterID>& connected_now = {}) const;

    /// get a router contact of a bootstrap snode to bootstrap with if we are in need of
    /// bootstrapping more peers. pass in an unodereed set of bootstrap router pubkeys we are
    /// currently connected to.
    std::optional<RouterContact>
    select_bootstrap(const std::unordered_set<RouterID>& connected_now = {}) const;
  };
}  // namespace llarp::consensus
