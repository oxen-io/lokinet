#pragma once

#include "hopid.hpp"
#include "path_handler.hpp"
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
    // This class is the top-level holder of all paths and transit hops, and has a primary purpose
    // of being able to look up the associated path/transit hop on incoming traffic.
    class PathContext
    {
      private:
        Router& _r;

        using Lock_t = util::NullLock;
        mutable util::NullMutex paths_mutex;

        // Paths/TransitHops are 1:1 with edge rxIDs
        std::unordered_map<HopID, std::shared_ptr<Path>> _path_map;
        std::unordered_map<HopID, std::shared_ptr<TransitHop>> _transit_hops;

        bool _allow_transit{false};

      public:
        explicit PathContext(Router& r);

        std::tuple<size_t, size_t> path_ctx_stats() const;

        bool has_transit_hop(const TransitHop& hop) const;

        bool has_transit_hop(const HopID& hop_id) const;

        void put_transit_hop(std::shared_ptr<TransitHop> hop);

        Path* get_path(const HopID& hop_id) const;

        TransitHop* get_transit_hop(const HopID&) const;
        std::shared_ptr<TransitHop> get_transit_hop_ptr(const HopID&) const;

        void add_path(std::shared_ptr<Path> p);

        void drop(const Path& p);

        // TODO FIXME: currently unused, but it would be nice to allow clients to terminate a path
        // early (i.e. just before dropping all their connections), which will need this:
        void drop(const TransitHop& thop);

        void expire_hops(std::chrono::milliseconds now);

        void allow_transit();

        void reject_transit();

        bool is_transit_allowed() const;
    };
}  // namespace llarp::path
