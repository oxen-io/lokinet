#include "path_context.hpp"

#include "path.hpp"

#include <llarp/router/router.hpp>

namespace llarp::path
{
    static auto logcat = log::Cat("pathctx");

    PathContext::PathContext(Router& r) : _r{r} {}

    void PathContext::allow_transit() { _allow_transit = true; }

    bool PathContext::is_transit_allowed() const { return _allow_transit; }

    void PathContext::add_path(std::shared_ptr<Path> path) { _path_map.emplace(path->edge().rxid, std::move(path)); }

    void PathContext::expire_hops(std::chrono::milliseconds now)
    {
        assert(_r.loop.inside());
        int n = 0;
        for (auto it = _transit_hops.begin(); it != _transit_hops.end();)
        {
            if (it->second && it->second->is_expired(now))
            {
                it->second->is_dead = true;
                it = _transit_hops.erase(it);
                n++;
            }
            else
                ++it;
        }

        if (n > 0)
            log::debug(logcat, "{} expired TransitHops purged", n);
    }

    void PathContext::drop(const Path& path)
    {
        assert(_r.loop.inside());
        auto it = _path_map.find(path.edge().rxid);
        if (it != _path_map.end())
        {
            if (it->second)
                it->second->is_dead = true;
            _path_map.erase(it);
        }
    }

    void PathContext::drop(const TransitHop& thop)
    {
        assert(_r.loop.inside());
        for (const HopID* h : {&thop.txid, &thop.rxid})
        {
            auto it = _transit_hops.find(*h);
            if (it != _transit_hops.end())
            {
                if (it->second)
                    it->second->is_dead = true;
                _transit_hops.erase(it);
            }
        }
    }

    std::tuple<size_t, size_t> PathContext::path_ctx_stats() const
    {
        assert(_r.loop.inside());
        return {_path_map.size() / 2, _transit_hops.size() / 2};
    }

    bool PathContext::has_transit_hop(const TransitHop& hop) const
    {
        assert(_r.loop.inside());
        return has_transit_hop(hop.rxid) or has_transit_hop(hop.txid);
    }

    bool PathContext::has_transit_hop(const HopID& hop_id) const
    {
        assert(_r.loop.inside());
        return _transit_hops.count(hop_id);
    }

    void PathContext::put_transit_hop(std::shared_ptr<TransitHop> hop)
    {
        assert(_r.loop.inside());
        _transit_hops.emplace(hop->rxid, hop);
        _transit_hops.emplace(hop->txid, std::move(hop));
    }

    template <typename T>
    static const std::shared_ptr<T> nullshptr{};

    TransitHop* PathContext::get_transit_hop(const HopID& path_id) const
    {
        assert(_r.loop.inside());
        if (auto itr = _transit_hops.find(path_id); itr != _transit_hops.end())
            return itr->second.get();

        return nullptr;
    }
    std::shared_ptr<TransitHop> PathContext::get_transit_hop_ptr(const HopID& path_id) const
    {
        assert(_r.loop.inside());
        if (auto itr = _transit_hops.find(path_id); itr != _transit_hops.end())
            return itr->second;

        return nullptr;
    }

    Path* PathContext::get_path(const HopID& hop_id) const
    {
        assert(_r.loop.inside());
        if (auto itr = _path_map.find(hop_id); itr != _path_map.end())
            return itr->second.get();

        return nullptr;
    }

}  // namespace llarp::path
