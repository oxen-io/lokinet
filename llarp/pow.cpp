#include "pow.hpp"

#include <cmath>

#include "crypto/crypto.hpp"

namespace llarp
{
  PoW::~PoW() = default;

  bool
  PoW::decode_key(const llarp_buffer_t& /*k*/, llarp_buffer_t* /*val*/)
  {
    // TODO: implement me
    return false;
  }

  std::string
  PoW::bt_encode() const
  {
    return ""s;
  }

  bool
  PoW::IsValid(llarp_time_t now) const
  {
    if (now - timestamp > extendedLifetime)
      return false;

    ShortHash digest;
    auto buf = bt_encode();

    // hash
    if (!crypto::shorthash(
            digest, reinterpret_cast<uint8_t*>(buf.data()), buf.size()))
      return false;
    // check bytes required
    uint32_t required = std::floor(std::log(extendedLifetime.count()));
    for (uint32_t idx = 0; idx < required; ++idx)
    {
      if (digest[idx])
        return false;
    }
    return true;
  }

  std::string
  PoW::ToString() const
  {
    return fmt::format(
        "[PoW timestamp={} lifetime={} nonce={}]",
        timestamp.count(),
        extendedLifetime.count(),
        nonce);
  }

}  // namespace llarp
