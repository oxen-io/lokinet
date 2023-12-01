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
#include <map>
#include <optional>
#include <set>
#include <unordered_set>
#include <utility>

namespace llarp
{
  struct Router;

  inline constexpr size_t RID_SOURCE_COUNT{12};
  inline constexpr size_t BOOTSTRAP_SOURCE_COUNT{50};

  inline constexpr size_t MIN_ACTIVE_RIDS{24};

  inline constexpr size_t MAX_RID_ERRORS{4};

  // when fetching rids, each returned rid must appear this number of times across
  inline constexpr int MIN_RID_FETCH_FREQ{6};
  // when fetching rids, the total number of accepted returned rids should be above this number
  inline constexpr int MIN_RID_FETCH_TOTAL{};
  // when fetching rids, the ratio of accepted:rejected rids must be above this ratio
  inline constexpr double GOOD_RID_FETCH_THRESHOLD{};

  inline constexpr int MAX_FETCH_ATTEMPTS{10};
  inline constexpr int MAX_BOOTSTRAP_FETCH_ATTEMPTS{5};

  inline constexpr auto REBOOTSTRAP_INTERVAL{1min};

  inline constexpr auto FLUSH_INTERVAL{5min};

  class NodeDB
  {
    Router& _router;
    const fs::path _root;
    const std::function<void(std::function<void()>)> _disk;

    llarp_time_t _next_flush_time;

    /******** RouterID/RouterContacts ********/

    /** RouterID mappings
        Both the following are populated in NodeDB startup with RouterID's stored on disk.
        - known_rids: meant to persist between lokinet sessions, and is only
          populated during startup and RouterID fetching. This is meant to represent the
          client instance's most recent perspective of the network, and record which RouterID's
          were recently "active" and connected to
        - known_rcs: populated during startup and when RC's are updated both during gossip
          and periodic RC fetching
        - rc_lookup: holds all the same rc's as known_rcs, but can be used to look them up by
          their rid. Deleting an rid key deletes the corresponding rc in known_rcs
    */
    std::unordered_set<RouterID> known_rids;
    std::unordered_set<RemoteRC> known_rcs;
    std::unordered_map<RouterID, const RemoteRC&> rc_lookup;

    /** RouterID lists
        - white: active routers
        - gray: fully funded, but decommissioned routers
        - green: registered, but not fully-staked routers
    */
    std::unordered_set<RouterID> router_whitelist;
    std::unordered_set<RouterID> router_greylist;
    std::unordered_set<RouterID> router_greenlist;

    // All registered relays (service nodes)
    std::unordered_set<RouterID> registered_routers;
    // timing (note: Router holds the variables for last rc and rid request times)
    std::unordered_map<RouterID, rc_time> last_rc_update_times;
    // if populated from a config file, lists specific exclusively used as path first-hops
    std::unordered_set<RouterID> _pinned_edges;
    // source of "truth" for RC updating. This relay will also mediate requests to the
    // 12 selected active RID's for RID fetching
    RouterID fetch_source;
    // set of 12 randomly selected RID's from the client's set of routers
    std::unordered_set<RouterID> rid_sources{};
    // logs the RID's that resulted in an error during RID fetching
    std::unordered_set<RouterID> fail_sources{};
    // tracks the number of times each rid appears in the above responses
    std::unordered_map<RouterID, int> fetch_counters{};
    // stores all RID fetch responses for greedy comprehensive processing
    // std::unordered_map<RouterID, std::unordered_set<RouterID>> fetch_rid_responses;

    /** Failure counters:
        - fetch_failures: tracks errors fetching RC's from the RC node and requesting RID's
          from the 12 RID sources. Errors in the individual RID sets are NOT counted towards
          this, their performance as a group is evaluated wholistically
        - bootstrap_failures: tracks errors fetching both RC's from bootstrasps and RID requests
          they mediate. This is a different counter as we only bootstrap in problematic cases
    */
    std::atomic<int> fetch_failures{0}, bootstrap_failures{0};

    std::atomic<bool> is_fetching_rids{false}, is_fetching_rcs{false},
        using_bootstrap_fallback{false};

    bool
    want_rc(const RouterID& rid) const;

    /// asynchronously remove the files for a set of rcs on disk given their public ident key
    void
    remove_many_from_disk_async(std::unordered_set<RouterID> idents) const;

    /// get filename of an RC file given its public ident key
    fs::path
    get_path_by_pubkey(RouterID pk) const;

    std::unique_ptr<BootstrapList> _bootstraps;

   public:
    explicit NodeDB(
        fs::path rootdir, std::function<void(std::function<void()>)> diskCaller, Router* r);

    /// in memory nodedb
    NodeDB();

    bool
    process_fetched_rcs(RouterID source, std::vector<RemoteRC> rcs, rc_time timestamp);

    void
    ingest_rid_fetch_responses(const RouterID& source, std::unordered_set<RouterID> ids = {});

    bool
    process_fetched_rids();

    void
    fetch_initial();

    //  RouterContact fetching
    void
    fetch_rcs(bool initial = false);
    void
    post_fetch_rcs(bool initial = false);
    void
    fetch_rcs_result(bool initial = false, bool error = false);

    //  RouterID fetching
    void
    fetch_rids(bool initial = false);
    void
    post_fetch_rids(bool initial = false);
    void
    fetch_rids_result(bool initial = false);

    //  Bootstrap fallback
    void
    fallback_to_bootstrap();

    // Populate rid_sources with random sample from known_rids. A set of rids is passed
    // if only specific RID's need to be re-selected; to re-select all, pass the member
    // variable ::known_rids
    void
    reselect_router_id_sources(std::unordered_set<RouterID> specific);

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

    std::unordered_set<RouterID>&
    pinned_edges()
    {
      return _pinned_edges;
    }

    std::unique_ptr<BootstrapList>&
    bootstrap_list()
    {
      return _bootstraps;
    }

    void
    set_bootstrap_routers(std::unique_ptr<BootstrapList> from_router);

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

    const std::unordered_set<RemoteRC>&
    get_rcs() const
    {
      return known_rcs;
    }

    // const std::unordered_map<RouterID, RemoteRC>&
    // get_rcs() const
    // {
    //   return known_rcs;
    // }

    const std::unordered_map<RouterID, rc_time>&
    get_last_rc_update_times() const
    {
      return last_rc_update_times;
    }

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
      return _router.loop()->call_get([visit, this]() mutable -> std::optional<RemoteRC> {
        std::vector<RemoteRC> rcs{known_rcs.begin(), known_rcs.end()};

        std::shuffle(rcs.begin(), rcs.end(), llarp::csrng);

        for (const auto& entry : known_rcs)
        {
          if (visit(entry))
            return entry;
        }

        return std::nullopt;
      });
    }

    // Updates `current` to not contain any of the elements of `replace` and resamples (up to
    // `target_size`) from population to refill it.
    template <typename T, typename RNG>
    void
    replace_subset(
        std::unordered_set<T>& current,
        const std::unordered_set<T>& replace,
        std::unordered_set<T> population,
        size_t target_size,
        RNG&& rng)
    {
      // Remove the ones we are replacing from current:
      current.erase(replace.begin(), replace.end());

      // Remove ones we are replacing, and ones we already have, from the population so that we
      // won't reselect them:
      population.erase(replace.begin(), replace.end());
      population.erase(current.begin(), current.end());

      if (current.size() < target_size)
        std::sample(
            population.begin(),
            population.end(),
            std::inserter(current, current.end()),
            target_size - current.size(),
            rng);
    }

    /// visit all known_rcs
    template <typename Visit>
    void
    VisitAll(Visit visit) const
    {
      _router.loop()->call([this, visit]() {
        for (const auto& item : known_rcs)
          visit(item);
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
      _router.loop()->call([this, visit]() {
        std::unordered_set<RouterID> removed;

        for (auto itr = rc_lookup.begin(); itr != rc_lookup.end();)
        {
          if (visit(itr->second))
          {
            removed.insert(itr->first);
            itr = rc_lookup.erase(itr);
          }
          else
            ++itr;
        }

        if (not removed.empty())
          remove_many_from_disk_async(std::move(removed));
      });
    }

    /// remove rcs that are older than we want to keep.  For relays, this is when
    /// they  become "outdated" (i.e. 12hrs).  Clients will hang on to them until
    /// they are fully "expired" (i.e. 30 days), as the client may go offline for
    /// some time and can still try to use those RCs to re-learn the network.
    void
    remove_stale_rcs();

    /// put (or replace) the RC if we consider it valid (want_rc).  returns true if put.
    bool
    put_rc(RemoteRC rc, rc_time now = time_point_now());

    /// if we consider it valid (want_rc),
    /// put this rc into the cache if it is not there or is newer than the one there already
    /// returns true if the rc was inserted
    bool
    put_rc_if_newer(RemoteRC rc, rc_time now = time_point_now());
  };
}  // namespace llarp
