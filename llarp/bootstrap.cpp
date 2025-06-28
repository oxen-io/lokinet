#include "bootstrap.hpp"

#include "util/file.hpp"
#include "util/logging.hpp"
#include "util/logging/buffer.hpp"

#include <oxenc/bt_serialize.h>

namespace llarp
{
    static auto logcat = log::Cat("bootstrap");

    const RemoteRC& BootstrapList::current()
    {
        if (_bootstraps.empty())
            throw std::out_of_range{"No bootstraps available"};
        return _bootstraps[_curr % _bootstraps.size()];
    }
    const RemoteRC& BootstrapList::next()
    {
        if (_bootstraps.empty())
            throw std::out_of_range{"No bootstraps available"};
        ++_curr %= _bootstraps.size();
        return _bootstraps[_curr];
    }

    const RemoteRC& BootstrapList::next_with_shuffling()
    {
        if (_bootstraps.empty())
            throw std::out_of_range{"No bootstraps available"};
        if (++_curr == _bootstraps.size())
            shuffle();  // Resets _curr to 0
        return _bootstraps[_curr];
    }

    void BootstrapList::shuffle()
    {
        std::shuffle(_bootstraps.begin(), _bootstraps.end(), llarp::csrng);
        _curr = 0;
    }

    bool BootstrapList::contains(const RouterID& rid) const
    {
        return std::ranges::any_of(_bootstraps, [&rid](const auto& rc) { return rc.router_id() == rid; });
    }

    bool BootstrapList::contains(const RemoteRC& rc) const
    {
        return std::ranges::find(_bootstraps, rc) != _bootstraps.end();
    }

    void BootstrapList::clear()
    {
        _bootstraps.clear();
        _curr = 0;
    }

    size_t BootstrapList::add_fallbacks(NetID netid)
    {
        auto start_size = size();
        for (const auto& [n, rc_blob] : bootstrap_fallbacks)
        {
            if (n != netid)
                continue;
            add(netid, rc_blob, "Fallback bootstrap data");
        }
        return size() - start_size;
    }

    void BootstrapList::populate(
        NetID netid, const std::vector<fs::path>& paths, const fs::path& def, bool load_fallbacks)
    {
        for (const auto& f : paths)
        {
            log::debug(logcat, "Loading BootstrapRC from file {}", f);
            read_from_file(netid, f);
        }

        if (empty() && !def.empty() && fs::exists(def))
        {
            log::debug(logcat, "BootstrapRC list empty; looking for default from {}", def);
            try
            {
                read_from_file(netid, def);
            }
            catch (const std::exception& e)
            {
                log::warning(logcat, "Failed loading from default bootstrap file {}: {}.  Skipping it.", def, e.what());
            }
        }

        auto obsolete = std::erase_if(_bootstraps, [](const auto& bs) { return bs.is_obsolete(); });
        if (obsolete > 0)
            log::info(logcat, "Removed {} obsolete bootstraps RCs", obsolete);

        if (empty() and load_fallbacks)
        {
            log::debug(logcat, "Bootstrap list is empty; loading built-in fallbacks");
            auto size = add_fallbacks(netid);
            log::info(logcat, "Loaded {} {} default fallback bootstrap router contact(s)", size, netid);

            if (_bootstraps.empty())
            {
                log::error(
                    logcat,
                    "No Bootstrap routers were loaded.  The default Bootstrap file {} does not "
                    "exist, and this lokinet binary does not have any fallback Bootstrap RCs for the '{}' network.",
                    def,
                    netid);

                throw std::runtime_error("No Bootstrap nodes available.");
            }
        }

        // Shuffle whatever we loaded so that we're not always hitting the first one, or trying them
        // always in the same order.
        shuffle();

        log::debug(logcat, "We have {} Bootstrap router(s)!", size());
    }

    void BootstrapList::add(NetID netid, std::string_view data, std::string_view input_desc)
    {
        try
        {
            // Bootstrap data can container either a list of bootstraps, or just a single bootstrap RC:
            if (data.front() == 'l')
            {
                // list of bootstrap RCs
                for (oxenc::bt_list_consumer l{data}; !l.is_finished();)
                    _bootstraps.emplace_back(l.consume_dict_data(), netid, /*accept_expired=*/true);
            }
            else
            {
                // single bootstrap RC
                _bootstraps.emplace_back(data, netid, /*accept_expired=*/true);
            }
        }
        catch (const std::exception& e)
        {
            log::debug(
                logcat, "Failed to load the following bootstrap data from {}: {}", input_desc, buffer_printer{data});
            throw std::runtime_error{"{} does not contain valid bootstrap data: {}"_format(input_desc, e.what())};
        }
    }

    void BootstrapList::read_from_file(NetID netid, const fs::path& fpath)
    {
        if (not fs::exists(fpath))
            throw std::runtime_error{"Bootstrap RC file '{}' does not exist"_format(fpath)};

        auto content = util::file_to_string(fpath);
        if (content.empty())
            throw std::runtime_error{"Bootstrap RC file '{}' is empty"_format(fpath)};

        add(netid, content, "Bootstrap RC file '{}'"_format(fpath));

        log::debug(logcat, "Successfully loaded BootstrapRC file {} ({}B)", fpath, content.size());
    }

}  // namespace llarp
