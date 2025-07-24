#pragma once

#include <llarp/bootstrap.hpp>
#include <llarp/contact/relay_contact.hpp>
#include <llarp/contact/router_id.hpp>
#include <llarp/ev/types.hpp>
#include <llarp/util/thread/threading.hpp>

#include <atomic>
#include <filesystem>
#include <optional>
#include <set>
#include <unordered_set>
#include <utility>

namespace llarp
{
    class Router;

    // TESTNET: the following constants have been shortened for testing purposes

    inline constexpr auto FETCH_INTERVAL{10min};
    inline constexpr auto PURGE_INTERVAL{5min};
    inline constexpr auto FLUSH_INTERVAL{5min};

    /*  RC Fetch Constants  */
    // fallback to bootstrap if we have less than this many RCs
    inline constexpr int MIN_ACTIVE_RCS{6};
    // max number of attempts we make in non-bootstrap fetch requests
    inline constexpr int MAX_FETCH_ATTEMPTS{10};

    // when pro-actively fetching RCs, ask for this many for which we have RouterID but no RC
    inline constexpr int RC_FETCH_COUNT{5};

    // the total number of accepted returned rids should be above this number
    inline constexpr size_t MIN_GOOD_RID_FETCH_TOTAL{};
    // the ratio of accepted:rejected rids must be above this ratio
    inline constexpr double GOOD_RID_FETCH_THRESHOLD{};

    /*  RID Fetch Constants  */
    // the number of rid sources that we make rid fetch requests to
    inline constexpr size_t RID_SOURCE_COUNT{5};
    // upper limit on how many rid fetch requests to rid sources can fail
    inline constexpr int MAX_RID_ERRORS{1};
    // each returned rid must appear this number of times across all responses
    inline constexpr int MIN_RID_FETCH_FREQ{6};  //  TESTNET:

    /*  Bootstrap Constants  */
    // the number of rc's we query the bootstrap for; service nodes pass 0, which means
    // gimme all dat RCs
    inline constexpr size_t SERVICE_NODE_BOOTSTRAP_SOURCE_COUNT{0};
    inline constexpr size_t CLIENT_BOOTSTRAP_SOURCE_COUNT{10};

    // if all bootstraps fail, router will trigger re-bootstrapping after this cooldown
    inline constexpr auto FETCH_ATTEMPT_INTERVAL{15s};
    inline constexpr auto FETCH_ATTEMPTS{1};

    /*  Other Constants  */
    // threshold net number of verifications needed to promote an RID to known (positive) or drop
    // (negative) an unconfirmed rid.  Each observation or omission contributes +1 or -1 vote until
    // we have ± this threshold.
    inline constexpr int CONFIRMATION_THRESHOLD{3};

    class NodeDB
    {
        friend class Router;

        Router& _router;
        const fs::path _root;

        /******** RouterID/RelayContacts ********/

        using Lock_t = util::NullLock;
        mutable util::NullMutex nodedb_mutex;

        /** RouterID mappings
            Both the following are populated in NodeDB startup with RouterID's stored on disk.
            - known_rids: meant to persist between lokinet sessions, and is only
              populated during startup and RouterID fetching. This is meant to represent the
              client instance's most recent perspective of the network, and record which RouterID's
              were recently "active" and connected to
            - unconfirmed_rids: holds new rids returned in fetch requests to be verified by
           subsequent fetch requests
            - known_rcs: populated during startup and when RC's are updated both during gossip
              and periodic RC fetching
            - bootstrap_seeds: if we are the seed node, we insert the rc's of bootstrap fetch
           requests senders into this container to "introduce" them to each other
            - _bootstraps: the standard container for bootstrap RemoteRCs
        */
        std::set<RouterID> known_rids;
        std::unordered_map<RouterID, int> unconfirmed_rids;  // Value is the number of votes: seeing
                                                             // the rid is +1, missing it is -1.

        std::unordered_map<RouterID, RemoteRC> known_rcs;

        BootstrapList _bootstraps{};

        // All registered relays (service nodes)
        std::unordered_set<RouterID> _registered_routers;

        // if populated from a config file, lists specific exclusively used as path first-hops
        std::unordered_set<RouterID> _pinned_edges;

        // if true, ONLY use pinned edges for first hop
        bool _strict_connect{false};

        // set of 8 randomly selected RID's from the client's set of routers
        std::unordered_set<RouterID> rid_sources{};
        // logs the RID's that resulted in an error during RID fetching
        std::unordered_set<RouterID> fail_sources{};
        // tracks the number of times each rid appears in the above responses
        std::unordered_map<RouterID, std::atomic<int>> rid_result_counters{};

        std::atomic<int> fetch_counter{};
        std::atomic<int> fail_counter{};
        std::atomic<int> response_counter{};

        /// asynchronously remove the files for a set of rcs on disk given their public ident key
        void remove_many_from_disk_async(const std::vector<RouterID>& idents) const;

        /// get filename of an RC file given its public ident key
        fs::path get_path_by_pubkey(const RouterID& pk) const;

        // TESTNET: NEW MEMBERS FOR BOOTSTRAPPING MANAGED BY EVENTTRIGGER OBJECT
        std::atomic<bool> _needs_bootstrap{false}, _is_bootstrapping{false}, _has_bstrap_connection{false},
            _is_connecting_bstrap{false};

        std::shared_ptr<EventTrigger> _bootstrap_handler;

        std::shared_ptr<quic::Ticker> _rid_fetch_ticker;
        std::shared_ptr<quic::Ticker> _rc_fetch_ticker;

        std::shared_ptr<quic::Ticker> _purge_ticker;
        std::shared_ptr<quic::Ticker> _flush_ticker;

      public:
        explicit NodeDB(Router& r);

        bool strict_connect_enabled() const { return _strict_connect; }

        void start_tickers();

        // returns {num_rcs, num_rids, num_bootstraps}
        std::tuple<size_t, size_t, size_t> db_stats() const;

        const std::set<RouterID>& get_known_rids() const { return known_rids; }

        const std::unordered_map<RouterID, RemoteRC>& get_known_rcs() const { return known_rcs; }

        bool is_bootstrapping() const { return _is_bootstrapping; }
        bool needs_bootstrap() const { return _needs_bootstrap; }
        bool bootstrap_completed() const { return not(_is_bootstrapping or _needs_bootstrap); }
        bool is_bootstrap_node(const RemoteRC& rc) const;
        void purge_rcs(std::chrono::milliseconds now = llarp::time_now_ms());

        void set_router_whitelist(const std::vector<RouterID>& whitelist);

        std::optional<RouterID> get_random_registered_router() const;

        // client:
        //   if pinned edges were specified, connections are allowed only to those and
        //   to the configured bootstrap nodes.  otherwise, always allow.
        //
        // relay:
        //   outgoing connections are allowed only to other registered, funded relays
        //   (whitelist and greylist, respectively).
        bool is_connection_allowed(const RouterID& remote) const;

        // client:
        //   same as is_connection_allowed
        //
        // server:
        //   we only build new paths through registered, not decommissioned relays
        //   (i.e. whitelist)
        bool is_path_allowed(const RouterID& remote) const { return known_rids.count(remote); }

        // if pinned edges were specified, the remote must be in that set, else any remote
        // is allowed as first hop.
        bool is_first_hop_allowed(const RouterID& remote) const;

        const std::unordered_set<RouterID>& pinned_edges() const { return _pinned_edges; }

        // Sets the bootstrap list in strict-connect, pinned-edge mode, where we will only make
        // paths starting with the given router IDs.
        void set_pinned_edges(std::unordered_set<RouterID> edges);

        void bootstrap_init();

        size_t num_bootstraps() const { return _bootstraps.size(); }

        bool has_bootstraps() const { return _bootstraps.empty(); }

        const BootstrapList& bootstrap_list() const { return _bootstraps; }

        const std::unordered_set<RouterID>& registered_routers() const { return _registered_routers; }

        /// load all known_rcs from disk synchronously
        void load_from_disk();

        /// explicit save all RCs to disk synchronously
        void save_to_disk() const;

        /// called on close
        void cleanup();

        /// the number of known RC's currently held
        size_t num_rcs() const;

        size_t num_rids() const;

        /// do periodic tasks like flush to disk and expiration
        bool tick(std::chrono::milliseconds now);

        /// find the `num_routers` routers closest to dht key.  The order of returned elements is
        /// arbitrary (that is: they will be the N closest routers, but in no particular order).
        std::vector<const RemoteRC*> find_many_closest_to(hash_key location, int num_routers) const;

        /// return true if we have an rc by its ident pubkey
        bool has_rc(const RouterID& pk) const { return get_rc(pk); }

        /// maybe get an rc by its ident pubkey.  Returns nullptr if not found.
        const RemoteRC* get_rc(const RouterID& pk) const;

        /// Selects a random RC from all known RCs that return true from the given predicate (from
        /// all known RCs if no predicate is given).  Returns nullptr if there are no acceptable
        /// RCs.
        const RemoteRC* get_random_rc(const std::function<bool(const RemoteRC&)>& predicate = nullptr) const;

        /// Selects n random RCs from all known RCs (if a predicate is given, all that return true
        /// from the given predicate).  If there are fewer than `n` admissable RCs then all
        /// admissable RCs are returned.  The resulting RCs will also be shuffled before being
        /// returned, unless the shuffle argument is set to false.
        std::vector<std::reference_wrapper<const RemoteRC>> get_n_random_rcs(
            int n, bool shuffle = true, const std::function<bool(const RemoteRC&)>& predicate = nullptr) const;

        /// put (or replace) the RC if has a known valid RouterID, and we either don't have an RC,
        /// or the given rc is newer than what we have.  Returns true if put.
        bool put_rc(RemoteRC rc);

        bool verify_store_gossip_rc(const RemoteRC& rc);

      private:
        void fetch_rcs();
        void fetch_rids();
        void bootstrap();

        void post_rid_fetch(bool shutdown = false);

        void stop_bootstrap(bool success);

        /// remove any stored RCs matching the given predicate
        void remove_rcs_if(const std::function<bool(const RemoteRC&)>& remove);

        void handle_fetched_router_ids(const std::unordered_map<RouterID, std::set<RouterID>>& results);
    };
}  // namespace llarp
