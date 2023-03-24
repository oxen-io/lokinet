#include "os_traffic.hpp"
#include <cstdint>
#include <vector>

namespace llarp::layers::platform
{
  OSTraffic::OSTraffic(
      EtherType_t ethertype, PlatformAddr src_, PlatformAddr dst_, std::vector<uint8_t> data_)
      : kind{ethertype}, src{std::move(src_)}, dst{std::move(dst_)}, data{std::move(data_)}
  {}

  void
  OSTraffic::recompute_checksums()
  {
    // todo: implement me
  }

  std::string
  OSTraffic::ToString() const
  {
    return fmt::format("[OSTraffic kind={} src={} dst={} {} bytes]", kind, src, dst, data.size());
  }
}  // namespace llarp::layers::platform
