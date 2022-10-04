#pragma once

#include "router_contact.hpp"
#include <set>
#include <unordered_map>
#include "llarp/util/fs.hpp"

namespace llarp
{
  struct BootstrapList final : public std::set<RouterContact>
  {
    bool
    BDecode(llarp_buffer_t* buf);

    bool
    BEncode(llarp_buffer_t* buf) const;

    void
    AddFromFile(fs::path fpath);

    void
    Clear();
  };

  std::unordered_map<std::string, BootstrapList>
  load_bootstrap_fallbacks();

}  // namespace llarp
