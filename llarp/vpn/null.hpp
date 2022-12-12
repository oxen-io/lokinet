#pragma once
#include "platform.hpp"
#include <unistd.h>

namespace llarp::vpn
{
  class NullInterface : public NetworkInterface
  {
    /// we use a pipe because it isnt going to poll itself
    int m_pipe[2];

   public:
    NullInterface(InterfaceInfo info) : NetworkInterface{std::move(info)}
    {
      ::pipe(m_pipe);
    }

    virtual ~NullInterface()
    {
      ::close(m_pipe[1]);
      ::close(m_pipe[0]);
    }

    int
    PollFD() const override
    {
      return m_pipe[0];
    }

    net::IPPacket
    ReadNextPacket() override
    {
      return net::IPPacket{};
    }

    /// write a packet to the interface
    /// returns false if we dropped it
    bool
    WritePacket(net::IPPacket) override
    {
      return true;
    }
  };

  class NopRouteManager : public IRouteManager
  {
   public:
    void AddRoute(net::ipaddr_t, net::ipaddr_t) override{};

    void DelRoute(net::ipaddr_t, net::ipaddr_t) override{};

    void
    AddDefaultRouteViaInterface(NetworkInterface&) override{};

    void
    DelDefaultRouteViaInterface(NetworkInterface&) override{};

    void
    AddRouteViaInterface(NetworkInterface&, IPRange) override{};

    void
    DelRouteViaInterface(NetworkInterface&, IPRange) override{};

    std::vector<net::ipaddr_t>
    GetGatewaysNotOnInterface(NetworkInterface&) override
    {
      return std::vector<net::ipaddr_t>{};
    };
  };

  class NullPlatform : public Platform
  {
    NopRouteManager _routes;

   public:
    NullPlatform() : Platform{}, _routes{}
    {}

    virtual ~NullPlatform() = default;

    std::shared_ptr<NetworkInterface>
    ObtainInterface(InterfaceInfo info, AbstractRouter*) override
    {
      return std::make_shared<NullInterface>(std::move(info));
    }

    IRouteManager&
    RouteManager() override
    {
      return _routes;
    }
  };
}  // namespace llarp::vpn
