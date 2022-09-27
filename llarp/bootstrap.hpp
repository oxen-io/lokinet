#pragma once

#include "router_contact.hpp"
#include <set>
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
}  // namespace llarp
