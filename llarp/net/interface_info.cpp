#include "interface_info.hpp"

#include <llarp/util/str.hpp>

namespace llarp::net
{
  std::string
  InterfaceInfo::ToString() const
  {
    return fmt::format(
        "{}[i={}; addrs={}; gw={}]",
        name,
        index,
        llarp::join(",", addrs),
        gateway ? net::ToString(*gateway) : "none");
  }
}  // namespace llarp::net
