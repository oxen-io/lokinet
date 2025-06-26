#include "path_context.hpp"

#include "path.hpp"

#include <llarp/router/router.hpp>

namespace llarp::path
{
    static auto logcat = log::Cat("pathctx");

    PathContext::PathContext(Router& r) : _r{r} {}

    void PathContext::allow_transit() { _allow_transit = true; }

    bool PathContext::is_transit_allowed() const { return _allow_transit; }

    void PathContext::add_path(std::shared_ptr<Path> path)
    {
        _path_map.emplace(path->edge().rxid(), path);
        _path_map.emplace(path->pivot().txid(), std::move(path));
    }

    void PathContext::drop_paths(std::vector<HopID> droplist)
    {
        assert(_r.loop()->inside());
        for (auto itr = droplist.begin(); itr != droplist.end(); itr = droplist.erase(itr))
            _drop_path(*itr);
    }

    void PathContext::expire_hops(std::chrono::milliseconds now)
    {
        assert(_r.loop()->inside());
        size_t n = 0;

        for (auto itr = _transit_hops.begin(); itr != _transit_hops.end();)
        {
            if (itr->second->is_expired(now))
            {
                itr = _transit_hops.erase(itr);
                n += 1;
            }
            else
                ++itr;
        }

        if (n)
            log::debug(logcat, "{} expired TransitHops purged!", n);
    }

    void PathContext::drop_path(const Path& path)
    {
        assert(_r.loop()->inside());
        _drop_path(path.edge().rxid());
        _drop_path(path.pivot().txid());
    }

    std::tuple<size_t, size_t> PathContext::path_ctx_stats() const
    {
        assert(_r.loop()->inside());
        return {_path_map.size() / 2, _transit_hops.size() / 2};
    }

    bool PathContext::has_transit_hop(const TransitHop& hop) const
    {
        assert(_r.loop()->inside());
        return has_transit_hop(hop.rxid()) or has_transit_hop(hop.txid());
    }

    bool PathContext::has_transit_hop(const HopID& hop_id) const
    {
        assert(_r.loop()->inside());
        return _transit_hops.count(hop_id);
    }

    void PathContext::put_transit_hop(std::shared_ptr<TransitHop> hop)
    {
        assert(_r.loop()->inside());
        _transit_hops.emplace(hop->rxid(), hop);
        _transit_hops.emplace(hop->txid(), hop);
    }

    template <typename T>
    static const std::shared_ptr<T> nullshptr{};

    const std::shared_ptr<TransitHop>& PathContext::get_transit_hop(const HopID& path_id) const
    {
        assert(_r.loop()->inside());
        if (auto itr = _transit_hops.find(path_id); itr != _transit_hops.end())
            return itr->second;

        return nullshptr<TransitHop>;
    }

    void PathContext::_drop_path(const HopID& hop_id)
    {
        assert(_r.loop()->inside());

        if (auto itr = _path_map.find(hop_id); itr != _path_map.end())
            _path_map.erase(itr);
    }

    const std::shared_ptr<Path>& PathContext::get_path(const HopID& hop_id) const
    {
        assert(_r.loop()->inside());
        if (auto itr = _path_map.find(hop_id); itr != _path_map.end())
            return itr->second;

        return nullshptr<Path>;
    }

    bool PathContext::has_path(const HopID& hop_id) const
    {
        assert(_r.loop()->inside());
        return _path_map.contains(hop_id);
    }

}  // namespace llarp::path
