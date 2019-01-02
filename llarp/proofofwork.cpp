#include <buffer.hpp>
#include <pow.hpp>

#include <cmath>

namespace llarp
{
  PoW::~PoW()
  {
  }

  bool
  PoW::DecodeKey(__attribute__((unused)) llarp_buffer_t k,
                 __attribute__((unused)) llarp_buffer_t* val)
  {
    // TODO: implement me
    return false;
  }

  bool
  PoW::BEncode(llarp_buffer_t* buf) const
  {
    // TODO: implement me
    if(!bencode_start_dict(buf))
      return false;
    return bencode_end(buf);
  }

  bool
  PoW::IsValid(shorthash_func hashfunc, llarp_time_t now) const
  {
    if(now - timestamp > (uint64_t(extendedLifetime) * 1000))
      return false;

    ShortHash digest;
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
