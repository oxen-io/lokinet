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
    std::set<RemoteRC>::iterator _curr;

    const RemoteRC&
    current()
    {
      return *_curr;
    }

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
      ++_curr;

      if (_curr == this->end())
        _curr = this->begin();

      return *_curr;
    }

    bool
    contains(const RemoteRC& rc);

    void
    randomize()
    {
      if (size() > 1)
        _curr = std::next(begin(), std::uniform_int_distribution<size_t>{0, size() - 1}(csrng));
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
