#include <llarp/service/tun.hpp>

namespace llarp
{
  namespace service
  {
    TunEndpoint::TunEndpoint(const std::string& ifname, llarp_router* r)
        : Endpoint("tun-" + ifname, r)
    {
      m_tunif = tuntap_init();
      tuntap_set_ifname(m_tunif, ifname.c_str());
    }
  }  // namespace service
}  // namespace llarp