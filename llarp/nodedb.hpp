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

  /*  RC Fetch Constants  */
  // max number of attempts we make in non-bootstrap fetch requests
  inline constexpr int MAX_FETCH_ATTEMPTS{10};
  // the total number of returned rcs that are held locally should be at least this
  inline constexpr size_t MIN_GOOD_RC_FETCH_TOTAL{};
  // the ratio of returned rcs found locally to to total returned should be above this ratio
  inline constexpr double MIN_GOOD_RC_FETCH_THRESHOLD{};

  /*  RID Fetch Constants  */
  inline constexpr size_t MIN_ACTIVE_RIDS{24};
  // the number of rid sources that we make rid fetch requests to
  inline constexpr size_t RID_SOURCE_COUNT{12};
  // upper limit on how many rid fetch requests to rid sources can fail
  inline constexpr size_t MAX_RID_ERRORS{4};
  // each returned rid must appear this number of times across all responses
  inline constexpr int MIN_RID_FETCH_FREQ{RID_SOURCE_COUNT - MAX_RID_ERRORS - 1};
  // the total number of accepted returned rids should be above this number
  inline constexpr size_t MIN_GOOD_RID_FETCH_TOTAL{};
  // the ratio of accepted:rejected rids must be above this ratio
  inline constexpr double GOOD_RID_FETCH_THRESHOLD{};
  /*  Bootstrap Constants  */
  // the number of rc's we query the bootstrap for
  inline constexpr size_t BOOTSTRAP_SOURCE_COUNT{50};
  // the maximum number of fetch requests we make across all bootstraps
  inline constexpr int MAX_BOOTSTRAP_FETCH_ATTEMPTS{5};
  // if all bootstraps fail, router will trigger re-bootstrapping after this cooldown
  inline constexpr auto BOOTSTRAP_COOLDOWN{1min};

  /*  Other Constants  */
  // the maximum number of RC/RID fetches that can pass w/o an unconfirmed rc/rid appearing
  inline constexpr int MAX_CONFIRMATION_ATTEMPTS{5};
  // threshold amount of verifications to promote an unconfirmed rc/rid
  inline constexpr int CONFIRMATION_THRESHOLD{3};

  inline constexpr auto FLUSH_INTERVAL{5min};

  template <
      typename ID_t,
      std::enable_if_t<std::is_same_v<ID_t, RouterID> || std::is_same_v<ID_t, RemoteRC>, int> = 0>
  struct Unconfirmed
  {
    const ID_t id;
    int attempts = 0;
    int verifications = 0;

    Unconfirmed() = delete;
    Unconfirmed(const ID_t& obj) : id{obj}
    {}
    Unconfirmed(ID_t&& obj) : id{std::move(obj)}
    {}

    int
    strikes() const
    {
      return attempts;
    }

    operator bool() const
    {
      return verifications == CONFIRMATION_THRESHOLD;
    }

    bool
    operator==(const Unconfirmed& other) const
    {
      return id == other.id;
    }

    bool
    operator<(const Unconfirmed& other) const
    {
      return id < other.id;
    }
  };

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
        - unconfirmed_rids: holds new rids returned in fetch requests to be verified by subsequent
          fetch requests
        - known_rcs: populated during startup and when RC's are updated both during gossip
          and periodic RC fetching
        - unconfirmed_rcs: holds new rcs to be verified by subsequent fetch requests, similar to
          the unknown_rids container
        - rc_lookup: holds all the same rc's as known_rcs, but can be used to look them up by
          their rid
    */
    std::set<RouterID> known_rids;
    std::set<Unconfirmed<RouterID>> unconfirmed_rids;

    std::set<RemoteRC> known_rcs;
    std::set<Unconfirmed<RemoteRC>> unconfirmed_rcs;

    std::map<RouterID, const RemoteRC&> rc_lookup;

    /** RouterID lists    // TODO: get rid of all these, replace with better decom/not staked sets
        - white: active routers
        - gray: fully funded, but decommissioned routers
        - green: registered, but not fully-staked routers
    */
    std::unordered_set<RouterID> router_whitelist;
    std::unordered_set<RouterID> router_greylist;
    std::unordered_set<RouterID> router_greenlist;

    // All registered relays (service nodes)
    std::set<RouterID> registered_routers;
    // timing (note: Router holds the variables for last rc and rid request times)
    std::unordered_map<RouterID, rc_time> last_rc_update_times;
    // if populated from a config file, lists specific exclusively used as path first-hops
    std::set<RouterID> _pinned_edges;
    // source of "truth" for RC updating. This relay will also mediate requests to the
    // 12 selected active RID's for RID fetching
    RouterID fetch_source;
    // set of 12 randomly selected RID's from the client's set of routers
    std::set<RouterID> rid_sources{};
    // logs the RID's that resulted in an error during RID fetching
    std::set<RouterID> fail_sources{};
    // tracks the number of times each rid appears in the above responses
    std::unordered_map<RouterID, int> fetch_counters{};

    /** Failure counters:
        - fetch_failures: tracks errors fetching RC's from the RC node and requesting RID's
          from the 12 RID sources. Errors in the individual RID sets are NOT counted towards
          this, their performance as a group is evaluated wholistically
        - bootstrap_failures: tracks errors fetching both RC's from bootstrasps and RID requests
          they mediate. This is a different counter as we only bootstrap in problematic cases
    */
    std::atomic<int> fetch_failures{0}, bootstrap_failures{0};

    std::atomic<bool> _using_bootstrap_fallback{false}, _needs_rebootstrap{false},
        _needs_initial_fetch{true};

    bool
    want_rc(const RouterID& rid) const;

    /// asynchronously remove the files for a set of rcs on disk given their public ident key
    void
    remove_many_from_disk_async(std::unordered_set<RouterID> idents) const;

    /// get filename of an RC file given its public ident key
    fs::path
    get_path_by_pubkey(RouterID pk) const;

    std::unique_ptr<BootstrapList> _bootstraps{};

   public:
    explicit NodeDB(
        fs::path rootdir, std::function<void(std::function<void()>)> diskCaller, Router* r);

    /// in memory nodedb
    NodeDB();

    const std::set<RouterID>&
    get_known_rids() const
    {
      return known_rids;
    }

    const std::set<RemoteRC>&
    get_known_rcs() const
    {
      return known_rcs;
    }

    std::optional<RemoteRC>
    get_rc_by_rid(const RouterID& rid);

    bool
    needs_initial_fetch() const
    {
      return _needs_initial_fetch;
    }

    bool
    needs_rebootstrap() const
    {
      return _needs_rebootstrap;
    }

    bool
    ingest_fetched_rcs(std::set<RemoteRC> rcs, rc_time timestamp);

    bool
    process_fetched_rcs(std::set<RemoteRC>& rcs);

    void
    ingest_rid_fetch_responses(const RouterID& source, std::set<RouterID> ids = {});

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

    //  Bootstrap fallback fetching
    void
    fallback_to_bootstrap();
    void
    bootstrap_cooldown();

    // Populate rid_sources with random sample from known_rids. A set of rids is passed
    // if only specific RID's need to be re-selected; to re-select all, pass the member
    // variable ::known_rids
    void
    reselect_router_id_sources(std::set<RouterID> specific);

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
      return known_rids.count(remote);
    }

    // if pinned edges were specified, the remote must be in that set, else any remote
    // is allowed as first hop.
    bool
    is_first_hop_allowed(const RouterID& remote) const;

    std::set<RouterID>&
    pinned_edges()
    {
      return _pinned_edges;
    }

    size_t
    num_bootstraps() const
    {
      return _bootstraps ? _bootstraps->size() : 0;
    }

    bool
    has_bootstraps() const
    {
      return _bootstraps ? _bootstraps->empty() : false;
    }

    const BootstrapList&
    bootstrap_list() const
    {
      return *_bootstraps;
    }

    BootstrapList&
    bootstrap_list()
    {
      return *_bootstraps;
    }

    void
    set_bootstrap_routers(std::unique_ptr<BootstrapList> from_router);

    const std::set<RouterID>&
    whitelist() const
    {
      return known_rids;
    }

    const std::unordered_set<RouterID>&
    greylist() const
    {
      return router_greylist;
    }

    const std::set<RouterID>&
    get_registered_routers() const
    {
      return registered_routers;
    }

    const std::set<RemoteRC>&
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
    num_rcs() const;

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

    std::optional<RemoteRC>
    get_random_rc() const;

    std::optional<std::vector<RemoteRC>>
    get_n_random_rcs(size_t n) const;

    /** The following random conditional functions utilize a simple implementation of reservoir
        sampling to return either 1 or n random RC's using only one pass through the set of RC's.

        Pseudocode:
          - begin iterating through the set
            - load the first n (or 1) that pass hook(n) into a list Selected[]
            - for all that pass the hook, increment i, tracking the number seen thus far
            - generate a random integer x from 0 to i
              - x < n ? Selected[x] = current : continue;
    */

    std::optional<RemoteRC>
    get_random_rc_conditional(std::function<bool(RemoteRC)> hook) const;

    std::optional<std::vector<RemoteRC>>
    get_n_random_rcs_conditional(size_t n, std::function<bool(RemoteRC)> hook) const;

    // Updates `current` to not contain any of the elements of `replace` and resamples (up to
    // `target_size`) from population to refill it.
    template <typename T, typename RNG>
    void
    replace_subset(
        std::set<T>& current,
        const std::set<T>& replace,
        std::set<T> population,
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
            known_rcs.erase(itr->second);
            itr = rc_lookup.erase(itr);
          }
          else
            ++itr;
        }

        if (not removed.empty())
          remove_many_from_disk_async(std::move(removed));
      });
    }

    template <
        typename ID_t,
        std::enable_if_t<std::is_same_v<ID_t, RouterID> || std::is_same_v<ID_t, RemoteRC>, int> = 0>
    void
    process_results(
        std::set<ID_t> unconfirmed, std::set<Unconfirmed<ID_t>>& container, std::set<ID_t>& known)
    {
      // before we add the unconfirmed set, we check to see if our local set of unconfirmed
      // rcs/rids appeared in the latest unconfirmed set; if so, we will increment their number
      // of verifications and reset the attempts counter. Once appearing in 3 different requests,
      // the rc/rid will be "verified" and promoted to the known_{rcs,rids} container
      for (auto itr = container.begin(); itr != container.end();)
      {
        auto& id = itr->id;
        auto& count = const_cast<int&>(itr->attempts);
        auto& verifications = const_cast<int&>(itr->verifications);

        if (auto found = unconfirmed.find(id); found != unconfirmed.end())
        {
          if (++verifications >= CONFIRMATION_THRESHOLD)
          {
            if constexpr (std::is_same_v<ID_t, RemoteRC>)
              put_rc_if_newer(id);
            else
              known.emplace(id);
            itr = container.erase(itr);
          }
          else
          {
            // reset attempt counter and continue
            count = 0;
            ++itr;
          }

          unconfirmed.erase(found);
        }

        itr = (++count >= MAX_CONFIRMATION_ATTEMPTS) ? container.erase(itr) : ++itr;
      }

      for (auto& id : unconfirmed)
      {
        container.emplace(std::move(id));
      }
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

namespace std
{
  template <>
  struct hash<llarp::Unconfirmed<llarp::RemoteRC>> : public hash<llarp::RemoteRC>
  {};

  template <>
  struct hash<llarp::Unconfirmed<llarp::RouterID>> : hash<llarp::RouterID>
  {};
}  // namespace std
