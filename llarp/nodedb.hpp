#pragma once

#include <llarp/contact/relay_contact.hpp>
#include <llarp/contact/router_id.hpp>
#include <llarp/util/thread/threading.hpp>

#include <atomic>
#include <filesystem>
#include <optional>
#include <shared_mutex>
#include <unordered_set>

namespace oxen::quic
{
    struct message;
    struct Ticker;
}  // namespace oxen::quic

namespace llarp
{
    class Router;

    inline constexpr auto FETCH_INTERVAL{10min};
    inline constexpr auto PURGE_INTERVAL{5min};

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

    // After a bootstrap (success or failure) that results in not enough RCs, this is how long we
    // wait before bootstrapping again.  In the case of repeated failures, we apply an linear
    // backoff in incrments of this value up to BOOTSTRAP_COOLDOWN_MAX.
    inline constexpr auto BOOTSTRAP_COOLDOWN = 3s;
    inline constexpr auto BOOTSTRAP_COOLDOWN_MAX = 60s;

    /*  Other Constants  */
    // threshold net number of verifications needed to promote an RID to known (positive) or drop
    // (negative) an unconfirmed rid.  Each observation or omission contributes +1 or -1 vote until
    // we have ± this threshold.
    inline constexpr int CONFIRMATION_THRESHOLD{3};

    class NodeDB
    {
        friend class Router;

        Router& _router;
        const std::filesystem::path _root;

        /******** RouterID/RelayContacts ********/

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
            - _bootstraps: the standard container for bootstrap RelayContacts
        */
        std::unordered_set<RouterID> known_rids;
        std::unordered_map<RouterID, int> unconfirmed_rids;  // Value is the number of votes: seeing
                                                             // the rid is +1, missing it is -1.

        std::unordered_map<RouterID, RelayContact> known_rcs;

        static const std::vector<std::pair<NetID, std::string_view>> bootstrap_fallbacks;
        std::vector<RelayContact> _bootstraps;
        void load_bootstraps();
        void load_bootstrap(const std::filesystem::path&);
        void load_bootstrap(std::string_view data, std::string_view log_desc);

        // All registered relays (service nodes)
        std::unordered_set<RouterID> _registered_relays;
        mutable std::shared_mutex _registered_relays_mutex;

        // set of 8 randomly selected RID's from the client's set of routers
        std::unordered_set<RouterID> rid_sources{};
        // logs the RID's that resulted in an error during RID fetching
        std::unordered_set<RouterID> fail_sources{};
        // tracks the number of times each rid appears in the above responses
        std::unordered_map<RouterID, std::atomic<int>> rid_result_counters{};

        std::atomic<int> fetch_counter{};
        std::atomic<int> fail_counter{};
        std::atomic<int> response_counter{};

        bool _bootstrap_running = false;
        int _bootstrap_fails = 0;

        /// asynchronously remove the files for a set of rcs on disk given their public ident key
        void remove_many_from_disk_async(const std::vector<RouterID>& idents) const;

        /// get filename of an RC file given its public ident key
        std::filesystem::path get_path_by_pubkey(const RouterID& pk) const;

        std::shared_ptr<quic::Ticker> _rid_fetch_ticker;
        std::shared_ptr<quic::Ticker> _rc_fetch_ticker;

        std::shared_ptr<quic::Ticker> _purge_ticker;

      public:
        explicit NodeDB(Router& r);

        // Starts the nodedb tickers for purge and fetch (clients), and initiates a bootstrap if the
        // nodedb has too few RCs.
        void start();

        // returns {num_rcs, num_rids, num_bootstraps}
        std::array<int, 3> db_stats() const;

        const std::unordered_set<RouterID>& get_known_rids() const { return known_rids; }

        const std::unordered_map<RouterID, RelayContact>& get_known_rcs() const { return known_rcs; }

        void purge_rcs(std::chrono::milliseconds now = llarp::time_now_ms());

        void set_registered_relays(std::unordered_set<RouterID> relays);
        bool has_registered_relays() const;
        std::vector<RouterID> get_registered_relays() const;

        // Called if our initial oxend SN request fails to load the router IDs of any RCs in our
        // nodedb as our initial registered relay list until some future oxend update comes along to
        // correct the list.
        void load_registered_relays_fallback();

        std::optional<RouterID> get_random_registered_relay() const;

        const std::unordered_set<RouterID>& strict_edges() const;

        int num_bootstraps() const { return static_cast<int>(_bootstraps.size()); }

        bool has_bootstraps() const { return !_bootstraps.empty(); }

        // Returns true if `relay` is a registered relay.  This uses a mutex (rather that event
        // loop) protection so that it can be safely called from either event loop without risking a
        // deadlock between the loops.
        bool is_registered(const RouterID& relay) const;

        /// load all known_rcs from disk synchronously
        void load_from_disk();

        /// called on close
        void cleanup();

        /// the number of known RC's currently held.  If `include_self` is false then we subtract
        /// one if the current service node RC is included in the nodedb.
        int num_rcs(bool include_self = true) const;

        // The number of known RIDs.  For relays, this is the number of registered relays (as
        // received from oxend); for clients this is the number of known router IDs fetched from
        // relays.
        int num_rids() const;

        /// find the `num_relays` relays with IDs closest to the given blinded pubkey, in order
        /// from closest to Nth-closest.  Note that this searches all network-registered rids, even
        /// if we don't have the RC for that relay yet.
        std::vector<RouterID> find_many_closest_to(const PubKey& blinded_pk, int num_relays) const;

        /// return true if we have an rc by its ident pubkey
        bool has_rc(const RouterID& pk) const { return get_rc(pk); }

        /// maybe get an rc by its ident pubkey.  Returns nullptr if not found.
        const RelayContact* get_rc(const RouterID& pk) const;

        /// Selects n random RCs from all known, unexpired, non-blocklisted RCs (if a predicate is
        /// given, they must also pass the given predicate).  If this is a service node, it will not
        /// include its own RC.  If there are fewer than `n` admissable RCs then all admissable RCs
        /// are returned.  The resulting RCs will also be shuffled before being returned, unless the
        /// shuffle argument is set to false.  The returned pointers are guaranteed to be
        /// non-nullptr.
        std::vector<const RelayContact*> get_n_random_rcs(
            int n, bool shuffle = true, const std::function<bool(const RelayContact&)>& predicate = nullptr) const;

        /// Wrapper around get_n_random_rcs to select a single random RC.  Returns nullptr if there
        /// are no acceptable RCs.
        const RelayContact* get_random_rc(const std::function<bool(const RelayContact&)>& predicate = nullptr) const;

        /// Same as `get_n_random_rcs`, except that this only returns RCs that are eligible for
        /// direct connections.  For a relay, or a client not using strict edges, this is exactly
        /// the same as `get_n_random_rcs`, but when strict edges are active, only listed strict
        /// router IDs are considered.
        std::vector<const RelayContact*> get_n_random_edge_rcs(
            int n, bool shuffle = true, const std::function<bool(const RelayContact&)>& predicate = nullptr) const;

        /// Stores an RC broadcast to the network.  The return value indicates whether this RC
        /// should be re-broadcast to all connected relays (true) or not (false).  In particular,
        /// false does *not* necessarily mean that the RC was not updated, but could also simply
        /// mean that the RC update was not significant enough to warrant rebroadcasting.
        ///
        /// This function does *not* check that the RC's router ID is actually a valid service node:
        /// call `verify_store_gossip_rc` instead of this to also do that check.
        ///
        /// In particular, RC re-gossipping is determined by:
        /// - The RC must be for a relay we haven't recently received an RC for (i.e. we didn't have
        ///   it, or what we had was declared outdated (more than 12h old)).
        /// - Alternatively, an RC will also be gossipped if it is an important update for
        ///   reachability (i.e. changed IP or port, or other crucial RC properties).
        /// - Gossips will not be accepted if the currently stored RC for the relay is not at least
        ///   a minute older than the incoming one.
        ///
        /// If storing *our own* RC then this returns true if it was stored, false otherwise,
        /// because we always want to gossip to our peers when we update our own RC.
        bool put_rc(RelayContact rc);

        /// Checks of the relay in the given rc is a registered remote network relay (either active
        /// or decommissioned, and not ourself) and, if so, calls and returns put_rc with it.
        ///
        /// Returns true if the router ID is known *and* the rc was updated *and* the RC should be
        /// re-gossipped (see put_rc); returns false otherwise.
        bool verify_store_gossip_rc(RelayContact rc);

      private:
        void fetch_rcs();
        void fetch_rids();

        /// Initiate a bootstrap fetch attempt.  This will try to bootstrap once from each
        /// configured bootstrap node until bootstrapping succeeds, or all bootstraps have been
        /// tried.  `on_bootstrap_done` will be called when the attempt finishes with a boolean
        /// indicating whether bootstrapping was successful.
        ///
        /// While a bootstrap is running the regular rid- and rc-fetching routines are disabled.
        void bootstrap();
        void on_bootstrap_done(bool success);

        bool handle_bootstrap_result(const RouterID& source, std::string_view body);

        void post_rid_fetch(bool shutdown = false);

        /// remove any stored RCs matching the given predicate
        void remove_rcs_if(const std::function<bool(const RelayContact&)>& remove);

        void handle_fetched_router_ids(const std::unordered_map<RouterID, std::unordered_set<RouterID>>& results);
    };
}  // namespace llarp
