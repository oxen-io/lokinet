#include "interface_info.hpp"

namespace llarp::net
{
  std::string
  InterfaceInfo::ToString() const
  {
    return fmt::format(
        "{}[i={}; addrs={}; gw={}]",
        name,
        index,
        fmt::join(addrs, ","),
        gateway ? net::ToString(*gateway) : "none");
  }
}  // namespace llarp::net
