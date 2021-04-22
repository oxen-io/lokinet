#pragma once

#include "router_contact.hpp"
#include <set>

namespace llarp
{
  struct BootstrapList final : public std::set<RouterContact>
  {
    bool
    BDecode(llarp_buffer_t* buf);

    bool
    BEncode(llarp_buffer_t* buf) const;

    void
    Clear();
  };
}  // namespace llarp
