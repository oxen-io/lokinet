#pragma once

#include "path_types.hpp"

#include <llarp/address/address.hpp>
#include <llarp/contact/client_intro.hpp>
#include <llarp/ev/types.hpp>
#include <llarp/util/decaying_hashset.hpp>
#include <llarp/util/thread/threading.hpp>
#include <llarp/util/time.hpp>

#include <atomic>
#include <unordered_map>
#include <unordered_set>

namespace std
{
    template <>
    struct hash<std::pair<llarp::RouterID, llarp::HopID>>
    {
        size_t operator()(const std::pair<llarp::RouterID, llarp::HopID>& i) const noexcept
        {
            return hash<llarp::RouterID>{}(i.first) ^ hash<llarp::HopID>{}(i.second);
        }
    };
}  // namespace std

namespace llarp
{
    class Router;
    namespace path
    {
        // forward declare
        struct Path;
    }  // namespace path

    using path_build_success_hook = std::function<void(const std::shared_ptr<path::Path>&)>;
    using path_build_fail_hook = std::function<void(const std::shared_ptr<path::Path>&, int)>;

    namespace path
    {
        // maximum number of paths a path-set can maintain
        inline constexpr size_t MAX_PATHS{32};

        /// limiter for path builds
        /// prevents overload and such
        class BuildLimiter
        {
            util::DecayingHashSet<RouterID> _edge_limiter;

          public:
            /// attempt a build
            /// return true if we are allowed to continue
            bool Attempt(const RouterID& router);

            /// decay limit entries
            void Decay(std::chrono::milliseconds now);

            /// return true if this router is currently limited
            bool Limited(const RouterID& router) const;
        };

        /// Stats about all our path builds
        struct BuildStats
        {
            static constexpr double THRESHOLD{0.25};

            uint64_t attempts{};
            uint64_t success{};
            uint64_t build_fails{};  // path build failures
            uint64_t path_fails{};   // path failures post-build
            uint64_t timeouts{};

            std::chrono::milliseconds last_warn_time{0s};

            nlohmann::json ExtractStatus() const;

            void update(std::chrono::milliseconds now);

            std::string to_string() const;
            static constexpr bool to_string_formattable = true;
        };

        struct PathHandler : public std::enable_shared_from_this<PathHandler>
        {
            friend struct Path;

          private:
            std::unordered_map<RouterID, std::weak_ptr<Path>> path_cache;

            void path_build_backoff();

          protected:
            std::shared_ptr<quic::Ticker> _path_rotater;

            /// flag for ::Stop()
            std::atomic<bool> _running;

            const size_t num_paths_desired;
            BuildStats _build_stats;

            using Lock_t = util::NullLock;
            mutable util::NullMutex paths_mutex;

            // key: upstream rxid
            std::unordered_map<HopID, std::shared_ptr<Path>> _paths;

            /// return true if we hit our soft limit for building paths too fast on a first hop
            bool build_cooldown_hit(RouterID edge) const;

            void drop_path(const std::shared_ptr<Path>& p);

            virtual void path_died(const std::shared_ptr<Path>& p);

            virtual void path_build_failed(const std::shared_ptr<Path>& p, bool timeout = false);

            virtual void path_build_succeeded(const std::shared_ptr<Path>& p);

            void path_build_recursive(
                sorted_intro_set intros,
                NetworkAddress remote,
                std::function<void(const std::shared_ptr<Path>&, ClientIntro)> cb,
                bool keep_path);

            void path_build_recursive(
                int n_tries,
                RemoteRC rc,
                NetworkAddress remote,
                std::function<void(const std::shared_ptr<Path>&)> cb,
                bool keep_path);

            void path_build_onepass(
                std::shared_ptr<Path> new_path, path_build_success_hook success, path_build_fail_hook fail);

            virtual void rotate_paths() = 0;

            void rotate_paths(std::vector<RemoteRC> hops);

            virtual void path_rotation_succeeded(const std::shared_ptr<Path>& new_path);

            const std::shared_ptr<Path>& get_oldest_path() const;

            const std::shared_ptr<Path>& get_newest_path() const;

            virtual void drop_oldest_path() = 0;

          public:
            Router& _router;
            int num_hops;
            std::chrono::milliseconds last_build{0s};
            std::chrono::milliseconds build_interval_limit{MIN_PATH_BUILD_INTERVAL};

            /// construct
            PathHandler(Router& _router, size_t num_paths, int num_hops = DEFAULT_LEN);

            virtual ~PathHandler() = default;

            /// get a shared_ptr of ourself
            virtual std::shared_ptr<PathHandler> get_self() = 0;

            /// get a weak_ptr of ourself
            virtual std::weak_ptr<PathHandler> get_weak() = 0;

            const Router& router() const { return _router; }

            Router& router() { return _router; }

            const std::shared_ptr<Path>& get_path(HopID id) const;

            sorted_intro_set get_local_client_intros() const;

            nlohmann::json ExtractStatus() const;

            virtual size_t should_build_more() const;

            void expire_paths(std::chrono::milliseconds now);

            void add_path(std::shared_ptr<Path> path);

            // Returns a random path, or nullptr if there are no paths.
            Path* get_random_active_path() const;

            // Returns the first path that matches the filter, or nullptr if no paths match.  Note
            // that the iteration order of paths is not guaranteed so if the filter could match
            // multiple paths it is not determined which you get.
            const std::shared_ptr<Path>& find_path(std::function<bool(const Path&)> filter) const;

            /// return true if we hit our soft limit for building paths too fast
            bool build_cooldown() const;

            /// get the number of ACTIVE paths in this status
            size_t num_active_paths() const;

            /// get the number of ALL paths (both active and those being currently build)
            size_t num_paths() const;

            const BuildStats& build_stats() const { return _build_stats; }

            BuildStats& build_stats() { return _build_stats; }

            virtual void stop(bool send_close = false);

            bool is_stopped() const;

            bool should_remove() const;

            std::chrono::milliseconds now() const;

            virtual void tick(std::chrono::milliseconds now);

            void tick_paths();

            void ping_paths(std::chrono::milliseconds now);

            // This method should be overridden by deriving classes
            virtual void build_more(size_t n = 0) = 0;

            bool build_path_to_random();

            bool build_path_aligned_to_remote(const RouterID& remote);

            std::optional<std::vector<RemoteRC>> aligned_hops_to_remote(const RouterID& pivot);

            // The build logic is segmented into functions designed to be called sequentially.
            //  - pre_build() : This handles all checking of the vector of hops, verifying with buildlimiter, etc
            //  - build1() : This can be re-implemented by inheriting classes that want to pass different parameters to
            //      the created path. This is useful in cases like OutboundSessions; Paths are constructed with the
            //      respective is_client and is_exit booleans set. Regardless, the implementation needs to return the
            //      created shared_ptr to be passed by reference to build2(...). The implementation MUST
            //      also check if the upstream rxid is already being used for a current path (very unlikely)
            //  - build2() : This contains the bulk of the code that is identical across all instances of path building.
            //      It returns the payload holding the encoded frames for each hop.
            //  - build() : This function calls pre_build() + build{1,2}() in the correct order and is used for the
            //      usual times that PathBuilder initiates a path build
            void build(std::vector<RemoteRC> hops);

            bool pre_build(const std::vector<RemoteRC>& hops);

            std::shared_ptr<Path> build1(const std::vector<RemoteRC>& hops);

            std::string build2(const std::shared_ptr<Path>& path);

            void for_each_path(std::function<void(Path&)> visit) const;

            /// pick a first hop
            std::optional<RemoteRC> select_first_hop(const std::unordered_set<RouterID>& exclude = {}) const;

            virtual std::optional<std::vector<RemoteRC>> get_hops_to_random();
        };
    }  // namespace path

}  // namespace llarp
