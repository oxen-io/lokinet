#ifndef LLARP_SERVICE_TUN_HPP
#define LLARP_SERVICE_TUN_HPP
#include <tuntap.h>
#include <llarp/service/endpoint.hpp>

namespace llarp
{
  namespace service
  {
    struct TunEndpoint : public Endpoint
    {
      TunEndpoint(const std::string& ifname, llarp_router* r);
      ~TunEndpoint();

      device* m_tunif;
    };
  }  // namespace service
}  // namespace llarp

#endif