#include "pow.hpp"

#include "crypto/crypto.hpp"
#include "util/buffer.hpp"

#include <cmath>

namespace llarp
{
  PoW::~PoW() = default;

  bool
  PoW::DecodeKey(const llarp_buffer_t& /*k*/, llarp_buffer_t* /*val*/)
  {
    // TODO: implement me
    return false;
  }

  bool
  PoW::BEncode(llarp_buffer_t* buf) const
  {
    // TODO: implement me
    if (!bencode_start_dict(buf))
      return false;
    return bencode_end(buf);
  }

  bool
  PoW::IsValid(llarp_time_t now) const
  {
    if (now - timestamp > extendedLifetime)
      return false;

    ShortHash digest;
    std::array<byte_t, MaxSize> tmp;
    llarp_buffer_t buf(tmp);
    // encode
    if (!BEncode(&buf))
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

  std::ostream&
  PoW::print(std::ostream& stream, int level, int spaces) const
  {
    Printer printer(stream, level, spaces);

    printer.printAttribute("pow timestamp", timestamp.count());
    printer.printAttribute("lifetime", extendedLifetime.count());
    printer.printAttribute("nonce", nonce);

    return stream;
  }

}  // namespace llarp
