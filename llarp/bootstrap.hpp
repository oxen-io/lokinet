#pragma once

#include "router_contact.hpp"

#include <llarp/crypto/crypto.hpp>
#include <llarp/util/fs.hpp>

#include <set>
#include <unordered_map>

namespace llarp
{
  struct BootstrapList final : public std::set<RemoteRC>
  {
    size_t index;
    std::set<RemoteRC>::iterator current;

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
    const RemoteRC&
    next()
    {
      ++current;

      if (current == this->end())
        current = this->begin();

      return *current;
    }

    bool
    contains(const RemoteRC& rc);

    void
    randomize()
    {
      current = std::next(begin(), std::uniform_int_distribution<size_t>{0, size() - 1}(csrng));
    }

    void
    clear_list()
    {
      clear();
    }
  };

  std::unordered_map<std::string, BootstrapList>
  load_bootstrap_fallbacks();

}  // namespace llarp
