#include "map.hpp"

namespace llarp
{

    void session_map::for_each(std::function<void(session::BaseSession&)> hook)
    {
        Lock_t l{session_mutex};

        for (auto& [_, s] : _sessions)
            if (s->is_active())
                hook(*s);
    }

    void session_map::tick_outbounds(std::chrono::milliseconds now)
    {
        Lock_t l{session_mutex};

        for (auto& [_, s] : _sessions)
            if (s->is_outbound() && s->is_active())
                s->tick_outbound(now);
    }

    void session_map::clear_sessions()
    {
        Lock_t l{session_mutex};
        _sessions.clear();
    }

    std::pair<std::shared_ptr<session::BaseSession>, bool> session_map::insert_or_assign(
        NetworkAddress remote, std::shared_ptr<session::BaseSession> sesh)
    {
        Lock_t l{session_mutex};

        auto [it1, b1] = _session_lookup.insert_or_assign(sesh->tag(), remote);
        auto [it2, b2] = _sessions.insert_or_assign(remote, std::move(sesh));

        it2->second->activate();
        return {it2->second, b1 & b2};
    }

    std::optional<NetworkAddress> session_map::get_remote(const session_tag& tag) const
    {
        Lock_t l{session_mutex};

        std::optional<NetworkAddress> ret = std::nullopt;

        if (auto itr = _session_lookup.find(tag); itr != _session_lookup.end())
            ret = itr->second;

        return ret;
    }

    std::shared_ptr<session::BaseSession> session_map::get_session(const NetworkAddress& remote) const
    {
        Lock_t l{session_mutex};

        std::shared_ptr<session::BaseSession> ret = nullptr;

        if (auto itr = _sessions.find(remote); itr != _sessions.end())
            ret = itr->second;

        return ret;
    }

    std::shared_ptr<session::BaseSession> session_map::get_session(const session_tag& tag) const
    {
        Lock_t l{session_mutex};

        std::shared_ptr<session::BaseSession> ret = nullptr;

        if (auto remote = get_remote(tag); remote != std::nullopt)
            ret = get_session(*remote);

        return ret;
    }

    void session_map::unmap(const session_tag& tag)
    {
        Lock_t l{session_mutex};

        if (auto it_a = _session_lookup.find(tag); it_a != _session_lookup.end())
        {
            if (auto it_b = _sessions.find(it_a->second); it_b != _sessions.end())
                _sessions.erase(it_b);

            _session_lookup.erase(it_a);
        }
    }

    void session_map::unmap(const NetworkAddress& remote)
    {
        Lock_t l{session_mutex};

        if (auto it_a = _sessions.find(remote); it_a != _sessions.end())
        {
            auto tag = it_a->second->tag();

            if (auto it_b = _session_lookup.find(tag); it_b != _session_lookup.end())
                _session_lookup.erase(it_b);

            _sessions.erase(it_a);
        }
    }

    bool session_map::have_session(const session_tag& tag) const
    {
        Lock_t l{session_mutex};

        if (auto itr = _session_lookup.find(tag); itr != _session_lookup.end())
            return have_session(itr->second);

        return false;
    }

}  // namespace llarp
