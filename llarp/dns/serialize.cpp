#include "serialize.hpp"
#include <oxenc/endian.h>
#include <cstdint>
#include <llarp/net/net_int.hpp>
#include <optional>
#include <stdexcept>
#include <vector>
#include "llarp/dns/rr.hpp"
#include "llarp/util/str.hpp"

namespace llarp
{

  dns::RR_RData_t
  encode_rdata(bstring_view rdata)
  {
    if (rdata.size() > 65536)
      throw std::invalid_argument{"rdata too big: {} > {}"_format(rdata.size(), 65536)};
    uint16_t len{static_cast<uint16_t>(rdata.size())};
    dns::RR_RData_t vec(rdata.size() + 2);
    oxenc::write_host_as_big(len, vec.data());
    std::copy_n(rdata.data(), rdata.size(), vec.data() + 2);
    return vec;
  }

  std::optional<bstring_view>
  maybe_decode_rdata(const RR_RData_t& vec)
  {
    if (vec.size() < 2)
      return std::nullopt;
    auto len = oxenc::load_big_to_host<uint16_t>(vec.data());
    if (vec.size() - 2 < len)
      return std::nullopt;
    return bstring_view{vec.data() + 2, size_t{len}};
  }
}  // namespace dns
}  // namespace llarp
