#include "edge_selector.hpp"

#include <llarp/router/abstractrouter.hpp>
#include <llarp/crypto/crypto.hpp>
#include <llarp/nodedb.hpp>
#include <llarp/profiling.hpp>

namespace llarp::consensus
{

  EdgeSelector::EdgeSelector(AbstractRouter& r) : _router{r}
  {}

  std::optional<RouterContact>
  EdgeSelector::select_path_edge(const std::unordered_set<RouterID>& connected_now) const
  {
    auto conf = _router.GetConfig();
    auto nodedb = _router.nodedb();

    const auto& _keys = conf->network.m_strictConnect;
    // no strict hops. select a random good snode.
    if (_keys.empty())
    {
      return nodedb->GetRandom([&connected_now, this](const auto& rc) {
        return connected_now.count(rc.pubkey) == 0
            and not _router.routerProfiling().IsBadForConnect(rc.pubkey)
            and not _router.IsBootstrapNode(rc.pubkey);
      });
    }

    // select random from strict connections.
    std::vector<RouterID> keys{_keys.begin(), _keys.end()};

    std::shuffle(keys.begin(), keys.end(), llarp::CSRNG{});

    for (const auto& pk : keys)
    {
      if (_router.routerProfiling().IsBadForConnect(pk))
        continue;
      if (connected_now.count(pk))
        continue;
      if (auto maybe = nodedb->Get(pk))
        return maybe;
    }
    return std::nullopt;
  }

  std::optional<RouterContact>
  EdgeSelector::select_bootstrap(const std::unordered_set<RouterID>& connected_now) const
  {
    auto conf = _router.GetConfig();
    auto nodedb = _router.nodedb();
    if (const auto& _keys = conf->network.m_strictConnect; not _keys.empty())
    {
      // try bootstrapping off strict connections first if we have any.
      std::vector<RouterID> keys{_keys.begin(), _keys.end()};
      std::shuffle(keys.begin(), keys.end(), llarp::CSRNG{});
      for (const auto& pk : keys)
      {
        if (connected_now.count(pk))
          continue;
        if (auto maybe = nodedb->Get(pk))
          return maybe;
      }
    }
    // if we cant use a strict connection we'll just use one of our bootstrap nodes.
    return nodedb->GetRandom([&connected_now, this](const auto& rc) {
      return connected_now.count(rc.pubkey) == 0 and _router.IsBootstrapNode(rc.pubkey);
    });
  }
}  // namespace llarp::consensus
