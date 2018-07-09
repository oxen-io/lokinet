#include <llarp/time.h>
#include <cmath>
#include <llarp/pow.hpp>
#include "buffer.hpp"

namespace llarp
{
  PoW::~PoW()
  {
  }

  bool
  PoW::BDecode(llarp_buffer_t* buf)
  {
    // TODO: implement me
    return false;
  }

  bool
  PoW::DecodeKey(llarp_buffer_t k, llarp_buffer_t* val)
  {
    // TODO: implement me
    return false;
  }

  bool
  PoW::BEncode(llarp_buffer_t* buf) const
  {
    // TODO: implement me
    return false;
  }

  bool
  PoW::IsValid(llarp_shorthash_func hashfunc) const
  {
    auto now = llarp_time_now_ms();

    if(now - timestamp > (uint64_t(extendedLifetime) * 1000))
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