#pragma once

#include "router_contact.hpp"

#include <llarp/util/fs.hpp>

#include <set>
#include <unordered_map>

namespace llarp
{
  struct BootstrapList final : public std::set<RemoteRC>
  {
    size_t index;

    bool
    bt_decode(std::string_view buf);

    std::string_view
    bt_encode() const;

    void
    read_from_file(const fs::path& fpath);

    bool
    contains(const RouterID& rid);

    // returns a reference to the next index and a boolean that equals true if
    // this is the front of the set
    std::pair<const RemoteRC&, bool>
    next()
    {
      ++index %= this->size();
      return std::make_pair(*std::next(this->begin(), index), index == 0);
    }

    bool
    contains(const RemoteRC& rc);

    void
    clear_list()
    {
      clear();
    }
  };

  std::unordered_map<std::string, BootstrapList>
  load_bootstrap_fallbacks();

}  // namespace llarp
