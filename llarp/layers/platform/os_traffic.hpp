#pragma once
#include <llarp/util/formattable.hpp>
#include "ethertype.hpp"
#include "llarp/net/ip_packet.hpp"
#include "platform_addr.hpp"
#include <cstdint>
#include <vector>

namespace llarp::layers::platform
{
  /// platfrom datum that is used to read and write to the os
  struct OSTraffic
  {
    /// the kind of traffic it is
    EtherType_t kind;
    /// platform level source and destination addresses of this traffic
    PlatformAddr src, dst;
    /// the datum itself
    std::vector<uint8_t> data;

    OSTraffic() = default;

    /// construct from a valid ip packet.
    explicit OSTraffic(net::IPPacket);

    OSTraffic(EtherType_t kind, PlatformAddr src, PlatformAddr dst, std::vector<uint8_t> data);

    std::string
    ToString() const;

   private:
    /// make sure all the checksums are correct.
    void
    recompute_checksums();
  };

}  // namespace llarp::layers::platform

namespace llarp
{
  template <>
  inline constexpr bool IsToStringFormattable<layers::platform::OSTraffic> = true;
}
