#pragma once

#include <llarp/contact/relay_contact.hpp>
#include <llarp/crypto/crypto.hpp>

#include <oxenc/bt_serialize.h>

#include <vector>

namespace llarp
{
    struct BootstrapList final
    {
      private:
        std::vector<RemoteRC> _bootstraps;
        size_t _curr = 0;

        void add(oxenc::bt_list_consumer&& l);
        void add(oxenc::bt_dict_consumer&& d);

        // Decodes either a list of RCs or a single RC and appends it/them to the bootstrap list.
        void add(NetID netid, std::string_view data, std::string_view input_desc);

        static const std::vector<std::pair<NetID, std::string_view>> bootstrap_fallbacks;
        size_t add_fallbacks(NetID netid);

        void read_from_file(NetID netid, const std::filesystem::path& fpath);

      public:
        BootstrapList() = default;

        // returns a reference to the current bootstrap in the list, without advancing.
        const RemoteRC& current();

        // advances to the next bootstrap in the list and returns it, wrapping around back to the
        // beginning when the end of the list is hit.
        const RemoteRC& next();

        // Similar to next, except when this hits the end it shuffles the list before returning to
        // the beginning of the new, shuffled list.
        const RemoteRC& next_with_shuffling();

        size_t size() const { return _bootstraps.size(); }
        bool empty() const { return _bootstraps.empty(); }

        bool contains(const RouterID& rid) const;
        bool contains(const RemoteRC& rc) const;

        // Shuffles the list of bootstraps and resets the current position to the beginning of the
        // new, shuffled list.
        void shuffle();

        void clear();

        void populate(
            NetID netid,
            const std::vector<std::filesystem::path>& paths,
            const std::filesystem::path& def);
    };

}  // namespace llarp
