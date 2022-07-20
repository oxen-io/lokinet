#pragma once
#include <llarp/ev/vpn.hpp>
#include "mock_network.hpp"

namespace mocks
{
  class MockInterface : public llarp::vpn::NetworkInterface
  {
    int _pipes[2];

   public:
    MockInterface(llarp::vpn::InterfaceInfo) : llarp::vpn::NetworkInterface{}
    {
      if (pipe(_pipes))
        throw std::runtime_error{strerror(errno)};
    }

    virtual ~MockInterface()
    {
      close(_pipes[1]);
    }

    int
    PollFD() const override
    {
      return _pipes[0];
    };

    std::string
    IfName() const override
    {
      return "ligma";
    };

    llarp::net::IPPacket
    ReadNextPacket() override
    {
      return llarp::net::IPPacket{};
    };

    bool WritePacket(llarp::net::IPPacket) override
    {
      return true;
    }
  };

  class MockVPN : public llarp::vpn::Platform, public llarp::vpn::IRouteManager
  {
    const Network& _net;

   public:
    MockVPN(const Network& net) : llarp::vpn::Platform{}, llarp::vpn::IRouteManager{}, _net{net}
    {}

    virtual std::shared_ptr<llarp::vpn::NetworkInterface>
    ObtainInterface(llarp::vpn::InterfaceInfo info, llarp::AbstractRouter*) override
    {
      return std::make_shared<MockInterface>(info);
    };

    const llarp::net::Platform*
    Net_ptr() const override
    {
      return &_net;
    };

    void AddRoute(IPVariant_t, IPVariant_t) override{};

    void DelRoute(IPVariant_t, IPVariant_t) override{};

    void AddDefaultRouteViaInterface(std::string) override{};

    void DelDefaultRouteViaInterface(std::string) override{};

    void
    AddRouteViaInterface(llarp::vpn::NetworkInterface&, llarp::IPRange) override{};

    void
    DelRouteViaInterface(llarp::vpn::NetworkInterface&, llarp::IPRange) override{};

    std::vector<IPVariant_t> GetGatewaysNotOnInterface(std::string) override
    {
      return std::vector<IPVariant_t>{};
    };

    /// get owned ip route manager for managing routing table
    virtual llarp::vpn::IRouteManager&
    RouteManager() override
    {
      return *this;
    };
  };
}  // namespace mocks
