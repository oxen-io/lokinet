#include "bootstrap.hpp"
#include "util/bencode.hpp"

namespace llarp
{
  void
  BootstrapList::Clear()
  {
    clear();
  }

  bool
  BootstrapList::BDecode(llarp_buffer_t* buf)
  {
    return bencode_read_list(
        [&](llarp_buffer_t* b, bool more) -> bool {
          if (more)
          {
            RouterContact rc;
            if (not rc.BDecode(b))
              return false;
            emplace(std::move(rc));
          }
          return true;
        },
        buf);
  }

  bool
  BootstrapList::BEncode(llarp_buffer_t* buf) const
  {
    return BEncodeWriteList(begin(), end(), buf);
  }
}  // namespace llarp
