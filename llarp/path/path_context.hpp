#pragma once

#include "path_handler.hpp"
#include "path_types.hpp"
#include "transit_hop.hpp"

#include <llarp/contact/client_contact.hpp>
#include <llarp/util/compare_ptr.hpp>
#include <llarp/util/decaying_hashset.hpp>

#include <memory>
#include <unordered_map>

namespace llarp
{
    class Router;
}

namespace llarp::path
{
    struct PathContext
    {
        explicit PathContext(Router& r);

      private:
        Router& _r;

        using Lock_t = util::NullLock;
        mutable util::NullMutex paths_mutex;

        // Paths are 1:1 with edge rxIDs
        std::unordered_map<HopID, std::shared_ptr<Path>> _path_map;

        std::unordered_map<HopID, std::shared_ptr<TransitHop>> _transit_hops;

        bool _allow_transit{false};

        // internal unsafe methods
        void _drop_path(const HopID& hop_id);

      public:
        std::tuple<size_t, size_t> path_ctx_stats() const;

        bool has_transit_hop(const TransitHop& hop) const;

        bool has_transit_hop(const HopID& hop_id) const;

        void put_transit_hop(std::shared_ptr<TransitHop> hop);

        Path* get_path(const HopID& hop_id) const;

        TransitHop* get_transit_hop(const HopID&) const;
        std::shared_ptr<TransitHop> get_transit_hop_ptr(const HopID&) const;

        void add_path(std::shared_ptr<Path> p);

        void drop_path(const Path& p);

        // Emplace both the edge().rxid() and the pivot().txid() into the droplist
        void drop_paths(std::vector<HopID> droplist);

        void expire_hops(std::chrono::milliseconds now);

        void allow_transit();

        void reject_transit();

        bool is_transit_allowed() const;
    };
}  // namespace llarp::path
