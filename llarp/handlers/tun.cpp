#include <llarp/service/tun.hpp>

namespace llarp
{
  namespace service
  {
    TunEndpoint::TunEndpoint(const std::string &ifname, llarp_router *r)
        : Endpoint("tunif-" + ifname, r)
        , m_tunif(tuntap_init())
        , m_IfName(ifname)
    {
    }

    TunEndpoint::~TunEndpoint()
    {
      tuntap_destroy(m_tunif);
    }

  }  // namespace service
}  // namespace llarp