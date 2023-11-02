#pragma once

#include "router_contact.hpp"

#include <llarp/util/fs.hpp>

#include <set>
#include <unordered_map>

namespace llarp
{
  struct BootstrapList final : public std::set<RemoteRC>
  {
    bool
    bt_decode(std::string_view buf);

    std::string_view
    bt_encode() const;

    void
    read_from_file(const fs::path& fpath);

    void
    clear_list()
    {
      clear();
    }
  };

  std::unordered_map<std::string, BootstrapList>
  load_bootstrap_fallbacks();

}  // namespace llarp
