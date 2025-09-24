#pragma once

#include "hopid.hpp"

#include <llarp/address/address.hpp>
#include <llarp/contact/client_intro.hpp>
#include <llarp/path/path.hpp>
#include <llarp/util/decaying_hashset.hpp>
#include <llarp/util/thread/threading.hpp>
#include <llarp/util/time.hpp>

#include <atomic>
#include <chrono>
#include <ranges>
#include <unordered_map>

namespace oxen::quic
{
    struct Ticker;
}

namespace llarp
{
    class Router;
    namespace path
    {
        /// We start delaying path builds once we hit this many consecutive path build failures:
        inline constexpr int BACKOFF_THRESHOLD = 3;

        /// Once we've met the above threshold, we apply a linear backoff starting with this delay
        /// and then increase the delay by this amount again for each additional path build failure.
        inline constexpr auto BACKOFF_INCREMENT = 1s;

        class PathHandler : public std::enable_shared_from_this<PathHandler>
        {
            void path_build_backoff();

          public:
            Router& router;

          protected:
            std::shared_ptr<quic::Ticker> _path_rotater;

            /// flag for ::Stop()
            std::atomic<bool> _running;

            int _num_hops;
            int _target_paths;
            int64_t _path_counter = 0;

            int _consecutive_failures = 0;
            std::chrono::milliseconds _last_failure = 0ms;
            std::chrono::milliseconds _last_build = 0ms;

            using Lock_t = util::NullLock;
            mutable util::NullMutex paths_mutex;

            // Container of paths.  The key is the hopid used by the edge when relaying messages
            // back to us along this path, i.e. the same as `value->edge().rxid`.
            std::unordered_map<HopID, std::shared_ptr<Path>> _paths;

            // Returns true if we are currently in the cooldown period because of path build
            // failures and thus should not currently be trying new path builds.
            bool cooldown(std::chrono::milliseconds now = llarp::time_now_ms()) const;

            void drop_path(const Path& p);

            virtual void path_died(const Path& p);

            /// Called when a path build fails.  The first argument is a unique non-zero integer as
            /// returned by build() and can be used to disambiguate the path that fails.  `path` is
            /// a pointer to the path object, but can be nullptr in the case of immediate failure
            /// (see below).  `timeout` will be true if the path build timed out, false if there was
            /// some other error.
            ///
            /// Note that this method can be called from within the build() call itself if a path
            /// build cannot even be attempted (i.e. for some immediate failure).  In such a case,
            /// the build_id will be 0 and the Path pointer will be nullptr.  For all other failure
            /// cases, the build_id value will be non-zero and the pointer will be non-nullptr.
            void path_build_failed(int64_t build_id, Path* path, bool timeout);

            /// Called during path_build_failed after performing basic path handling for subclasses
            /// to hook into path build failures.  The base class implementation does nothing.
            virtual void on_path_build_failure(int64_t /*build_id*/, Path* /*path*/, bool /*timeout*/) {}

            /// Called when a path build is successful and confirmed.  build_id is the non-zero
            /// integer as returned by the build() call that initiated the path build.
            void path_build_succeeded(int64_t build_id, Path& p);

            /// Called during path_build_succeeded after performing basic path handling for
            /// subclasses to hook into path build successes.  The base class implementation does
            /// nothing.
            virtual void on_path_build_success(int64_t /*build_id*/, Path& /*p*/) {}

          public:
            PathHandler(Router& router, int target_paths, int num_hops);

            virtual ~PathHandler() = default;

            Path* get_path_by_edge(const HopID& edge_hop_id);
            Path* get_path_by_terminus(const HopID& terminal_hop_id);

            nlohmann::json ExtractStatus() const;

            void expire_paths(std::chrono::milliseconds now);

            void add_path(Path& path);

            // Returns a random path, or nullptr if there are no paths.
            Path* get_random_active_path() const;

            /// get the number of ACTIVE, unexpired paths.  An future expiry value other than now
            /// can be given to query the number of active paths that will not have expired at the
            /// given timestamp.
            int num_active_paths(std::chrono::milliseconds expiry_ts = llarp::time_now_ms()) const;

            /// get the number of ALL unexpired paths (both active and those being currently built).
            /// If an expiry value is given then this returns the number of paths that will not have
            /// expired at that timestamp (i.e. passing in `llarp::time_now_ms() + 10s` will omit
            /// any paths expiring within the next 10 seconds).
            int num_paths(std::chrono::milliseconds expiry_ts = llarp::time_now_ms()) const;

            /// get the number of paths (active or currently building) to the given terminus relay
            int num_paths_to(const RouterID& terminus) const;

            /// Returns the target number of paths we attempt to maintain
            const int& target_paths() const { return _target_paths; }

            /// Returns the number of hops used for paths built by this object
            const int& num_hops() const { return _num_hops; }

            // TODO FIXME: this seems like an entangled mess: I don't think *anything* ever calls
            // this, except for Router calling SessionHandler::stop (which overrides this but then
            // calls it from the override).  But "send_close" has no apparent meaning here, and is
            // only in the base class because SessionHandler::stop's override uses it.
            void stop();

            bool is_stopped() const;

            /// Called each path handler tick to allow subclasses to perform path checks, updates,
            /// rotations, start new paths, etc. as needed.  If not overridden this does nothing.
            virtual void update_paths(std::chrono::milliseconds /*now*/) {}

            virtual void tick(std::chrono::milliseconds now);

            void ping_paths(std::chrono::milliseconds now);

            Path* build_path_to_remote(const RouterID& remote, std::chrono::seconds lifetime = path::MAX_LIFETIME);

            std::optional<std::vector<RelayContact>> select_hops_to_remote(const RouterID& pivot);

            /// Attempts to build the given path and send it to the network, initiating the path
            /// build.  When the build is done it calls either path_build_succeeded or
            /// path_build_failed.  It is possible for path_build_failed to fire *before* this
            /// function returns if the given path cannot currently be built (such as when shutting
            /// down, or if the rate limiter is hit).
            ///
            /// The return value is a unique id for the path that is passed into the
            /// path_build_failed/_succeeded methods to uniquely identify the path, or 0 if the path
            /// build is not currently possible.
            Path* build(
                std::span<const RelayContact> hops,
                std::chrono::milliseconds expiry_ts = llarp::time_now_ms() + path::MAX_LIFETIME);

            /// Returns a view over all current paths (as `Path&` references)
            auto paths() const
            {
                return std::views::values(_paths)  //
                    | std::views::filter(&std::shared_ptr<Path>::operator bool)
                    | std::views::transform(&std::shared_ptr<Path>::operator*);
            }

            /// Returns a view over all active paths (i.e. established and not expired)
            auto active_paths(std::chrono::milliseconds now = llarp::time_now_ms()) const
            {
                return std::views::values(_paths)  //
                    | std::views::filter([now](const std::shared_ptr<Path>& p) { return p && p->is_active(now); })
                    | std::views::transform(&std::shared_ptr<Path>::operator*);
            }

            /// pick a first hop; if predicate is given, only routers for which it returns true are
            /// permitted.  (Note that the path build limiter and router profile are always checked,
            /// regardless of the predicate).  Returns nullptr if no acceptable first hops are
            /// found.
            const RelayContact* select_first_hop(std::function<bool(const RelayContact&)> pred = nullptr) const;

          private:
            /// Checks whether we are currently able to build the given path (e.g. not stopped, the
            /// path edge is not build limited, valid number of hops).
            bool can_build(std::span<const RelayContact> hops);

            /// Takes a set of path hops (edge, hop1, hop2, ..., pivot) and initializes a Path
            /// following those hops, including generating path IDs that will be used along the
            /// path.
            std::shared_ptr<Path> build_init_path(
                std::span<const RelayContact> hops, std::chrono::milliseconds expiry_ts);

            /// Takes a path as constructed by build_init_path and constructs an encoded network
            /// path build message containing the frames required to build the path.
            std::vector<std::byte> path_build_onion(Path& path);

            /// Takes the path build (from encode_path_build) and fires it down the path.  When the
            /// path build finishes it calls either path_build_succeeded on success, or
            /// path_build_failed on failure.
            void send_path_build(const std::shared_ptr<Path>& new_path, int64_t id);

          public:
            // Counterpart to path_build_onion that decrypts a single path build frame; this is only
            // actually called from link_manager.cpp, but is here to be alongside the
            // path_build_onion that builds the frames.
            static std::pair<std::shared_ptr<path::TransitHop>, SymmNonce> decrypt_build_frame(
                std::span<const std::byte, path::BUILD_FRAME_SIZE> frame,
                const Router& r,
                const std::variant<RouterID, quic::ConnectionID>& src,
                std::chrono::milliseconds now);
        };
    }  // namespace path

}  // namespace llarp
