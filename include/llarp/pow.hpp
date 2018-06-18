#ifndef LLARP_POW_HPP
#define LLARP_POW_HPP
#include <llarp/buffer.h>
#include <llarp/crypto.h>
#include <llarp/router_id.hpp>

namespace llarp
{
  /// proof of work
  struct PoW
  {
    static constexpr size_t MaxSize = 128;

    RouterID router;
    uint64_t version          = 0;
    uint32_t extendedLifetime = 0;
    byte_t nonce[32];

    bool
    BEncode(llarp_buffer_t* buf) const;

    bool
    BDecode(llarp_buffer_t* buf);

    bool
    IsValid(llarp_shorthash_func hashfunc, const RouterID& us) const;
  };
}  // namespace llarp

#endif