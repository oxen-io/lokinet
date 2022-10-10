#pragma once
#include <llarp/router/router.hpp>

#include "mock_network.hpp"

namespace mocks
{
  class MockRouter : public llarp::Router
  {
    const Network& _net;

   public:
    explicit MockRouter(const Network& net, std::shared_ptr<llarp::vpn::Platform> vpnPlatform)
        : llarp::
            Router{std::shared_ptr<llarp::EventLoop>{const_cast<Network*>(&net), [](Network*) {}}, vpnPlatform}
        , _net{net}
    {}

    const llarp::net::Platform&
    Net() const override
    {
      return _net;
    };
  };
}  // namespace mocks
