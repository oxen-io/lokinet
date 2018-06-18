#include <cmath>
#include <llarp/pow.hpp>
#include "buffer.hpp"

namespace llarp
{
  bool
  PoW::BDecode(llarp_buffer_t* buf)
  {
    return false;
  }

  bool
  PoW::BEncode(llarp_buffer_t* buf) const
  {
    return false;
  }

  bool
  PoW::IsValid(llarp_shorthash_func hashfunc, const RouterID& us) const
  {
    // is it for us?
    if(router != us)
      return false;
    byte_t digest[SHORTHASHSIZE];
    byte_t tmp[MaxSize];
    auto buf = llarp::StackBuffer< decltype(tmp) >(tmp);
    // encode
    if(!BEncode(&buf))
      return false;
    // rewind
    buf.sz  = buf.cur - buf.base;
    buf.cur = buf.base;
    // hash
    if(!hashfunc(digest, buf))
      return false;
    // check bytes required
    uint32_t required = std::floor(std::log(extendedLifetime));
    for(uint32_t idx = 0; idx < required; ++idx)
    {
      if(digest[idx])
        return false;
    }
    return true;
  }

}  // namespace llarp