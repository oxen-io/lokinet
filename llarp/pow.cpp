#include "pow.hpp"

#include "crypto/crypto.hpp"
#include "util/buffer.hpp"

#include <cmath>

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
    std::array<byte_t, MaxSize> tmp;
    llarp_buffer_t buf(tmp);

    auto bte = bt_encode();

    if (auto b = buf.write(bte.begin(), bte.end()); not b)
      return false;

    // rewind
    buf.sz = buf.cur - buf.base;
    buf.cur = buf.base;
    // hash
    if (!CryptoManager::instance()->shorthash(digest, buf))
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
