#pragma once

#include "crypto/crypto.hpp"
#include "dht/key.hpp"
#include "router_contact.hpp"
#include "router_id.hpp"
#include "util/common.hpp"
#include "util/fs.hpp"
#include "util/thread/threading.hpp"

#include <llarp/router/router.hpp>

#include <algorithm>
#include <atomic>
#include <optional>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace llarp
{
  struct Router;

  class NodeDB
  {
    std::unordered_map<RouterID, RemoteRC> known_rcs;

    Router& router;
    const fs::path m_Root;
    const std::function<void(std::function<void()>)> disk;

    llarp_time_t m_NextFlushAt;

    /// asynchronously remove the files for a set of rcs on disk given their public ident key
    void
    remove_many_from_disk_async(std::unordered_set<RouterID> idents) const;

    /// get filename of an RC file given its public ident key
    fs::path
    get_path_by_pubkey(RouterID pk) const;

    std::unordered_map<RouterID, RemoteRC> bootstraps;

    // Router lists for snodes
    // whitelist = active routers
    std::unordered_set<RouterID> router_whitelist;
    // greylist = fully funded, but decommissioned routers
    std::unordered_set<RouterID> router_greylist;
    // greenlist = registered but not fully-staked routers
    std::unordered_set<RouterID> router_greenlist;
    // all registered relays (snodes)
    std::unordered_set<RouterID> registered_routers;
    std::unordered_map<RouterID, rc_time> last_rc_update_times;

    // Router list for clients
    std::unordered_set<RouterID> client_known_rcs;

    // only ever use to specific edges as path first-hops
    std::unordered_set<RouterID> pinned_edges;

    // rc update info
    RouterID rc_fetch_source;
    rc_time last_rc_update_relay_timestamp;
    std::unordered_set<RouterID> router_id_fetch_sources;
    std::unordered_map<RouterID, std::vector<RouterID>> router_id_fetch_responses;
    // process responses once all are received (or failed/timed out)
    size_t router_id_response_count{0};

    bool
    want_rc(const RouterID& rid) const;

   public:
    void
    set_bootstrap_routers(const std::set<RemoteRC>& rcs);

    const std::unordered_set<RouterID>&
    whitelist() const
    {
      return router_whitelist;
    }

    const std::unordered_set<RouterID>&
    greylist() const
    {
      return router_greylist;
    }

    const std::unordered_set<RouterID>&
    get_registered_routers() const
    {
      return registered_routers;
    }

    const std::unordered_map<RouterID, RemoteRC>&
    get_rcs() const
    {
      return known_rcs;
    }

    const std::unordered_map<RouterID, rc_time>&
    get_last_rc_update_times() const
    {
      return last_rc_update_times;
    }

    // If we receive a set of RCs from our current RC source relay, we consider
    // that relay to be a bad source of RCs and we randomly choose a new one.
    //
    // When using a new RC fetch relay, we first re-fetch the full RC list and, if
    // that aligns with our RouterID list, we go back to periodic updates from that relay.
    //
    // This will respect edge-pinning and attempt to use a relay we already have
    // a connection with.
    void
    rotate_rc_source();

    void
    ingest_rcs(RouterID source, std::vector<RemoteRC> rcs, rc_time timestamp);

    void
    ingest_router_ids(RouterID source, std::vector<RouterID> ids);

    void
    update_rcs();

    void
    set_router_whitelist(
        const std::vector<RouterID>& whitelist,
        const std::vector<RouterID>& greylist,
        const std::vector<RouterID>& greenlist);

    std::optional<RouterID>
    get_random_whitelist_router() const;

    // client:
    //   if pinned edges were specified, connections are allowed only to those and
    //   to the configured bootstrap nodes.  otherwise, always allow.
    //
    // relay:
    //   outgoing connections are allowed only to other registered, funded relays
    //   (whitelist and greylist, respectively).
    bool
    is_connection_allowed(const RouterID& remote) const;

    // client:
    //   same as is_connection_allowed
    //
    // server:
    //   we only build new paths through registered, not decommissioned relays
    //   (i.e. whitelist)
    bool
    is_path_allowed(const RouterID& remote) const
    {
      return router_whitelist.count(remote);
    }

    // if pinned edges were specified, the remote must be in that set, else any remote
    // is allowed as first hop.
    bool
    is_first_hop_allowed(const RouterID& remote) const;

    const std::unordered_set<RouterID>&
    get_pinned_edges() const
    {
      return pinned_edges;
    }

    explicit NodeDB(
        fs::path rootdir, std::function<void(std::function<void()>)> diskCaller, Router* r);

    /// in memory nodedb
    NodeDB();

    /// load all known_rcs from disk syncrhonously
    void
    load_from_disk();

    /// explicit save all RCs to disk synchronously
    void
    save_to_disk() const;

    /// the number of RCs that are loaded from disk
    size_t
    num_loaded() const;

    /// do periodic tasks like flush to disk and expiration
    void
    Tick(llarp_time_t now);

    /// find the absolute closets router to a dht location
    RemoteRC
    find_closest_to(dht::Key_t location) const;

    /// find many routers closest to dht key
    std::vector<RemoteRC>
    find_many_closest_to(dht::Key_t location, uint32_t numRouters) const;

    /// return true if we have an rc by its ident pubkey
    bool
    has_rc(RouterID pk) const;

    /// maybe get an rc by its ident pubkey
    std::optional<RemoteRC>
    get_rc(RouterID pk) const;

    template <typename Filter>
    std::optional<RemoteRC>
    GetRandom(Filter visit) const
    {
      return router.loop()->call_get([visit]() -> std::optional<RemoteRC> {
        std::vector<const decltype(known_rcs)::value_type*> known_rcs;
        for (const auto& entry : known_rcs)
          known_rcs.push_back(entry);

        std::shuffle(known_rcs.begin(), known_rcs.end(), llarp::csrng);

        for (const auto entry : known_rcs)
        {
          if (visit(entry->second))
            return entry->second;
        }

        return std::nullopt;
      });
    }

    /// visit all known_rcs
    template <typename Visit>
    void
    VisitAll(Visit visit) const
    {
      router.loop()->call([this, visit]() {
        for (const auto& item : known_rcs)
          visit(item.second);
      });
    }

    /// remove an entry via its ident pubkey
    void
    remove_router(RouterID pk);

    /// remove an entry given a filter that inspects the rc
    template <typename Filter>
    void
    RemoveIf(Filter visit)
    {
      router.loop()->call([this, visit]() {
        std::unordered_set<RouterID> removed;
        auto itr = known_rcs.begin();
        while (itr != known_rcs.end())
        {
          if (visit(itr->second))
          {
            removed.insert(itr->second.router_id());
            itr = known_rcs.erase(itr);
          }
          else
            ++itr;
        }
        if (not removed.empty())
          remove_many_from_disk_async(std::move(removed));
      });
    }

    /// remove rcs that are not in keep and have been inserted before cutoff
    void
    remove_stale_rcs(std::unordered_set<RouterID> keep, llarp_time_t cutoff);

    /// put (or replace) the RC if we consider it valid (want_rc).  returns true if put.
    bool
    put_rc(
        RemoteRC rc,
        rc_time now =
            std::chrono::time_point_cast<std::chrono::seconds>(std::chrono::system_clock::now()));

    /// if we consider it valid (want_rc),
    /// put this rc into the cache if it is not there or is newer than the one there already
    /// returns true if the rc was inserted
    bool
    put_rc_if_newer(
        RemoteRC rc,
        rc_time now =
            std::chrono::time_point_cast<std::chrono::seconds>(std::chrono::system_clock::now()));
  };
}  // namespace llarp
