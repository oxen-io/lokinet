#pragma once
#include <llarp/ev/vpn.hpp>
#include <unistd.h>

namespace llarp::vpn
{
  class NullInterface : public NetworkInterface
  {
    /// we use a pipe because it isnt going to poll itself
    int m_pipe[2];

   public:
    NullInterface()
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

    /// the interface's name
    std::string
    IfName() const override
    {
      return "";
    }

    net::IPPacket
    ReadNextPacket() override
    {
      return net::IPPacket{};
    }

    /// write a packet to the interface
    /// returns false if we dropped it
    bool WritePacket(net::IPPacket) override
    {
      return true;
    }
  };

  class NopRouteManager : public IRouteManager
  {
   public:
    void AddRoute(IPVariant_t, IPVariant_t) override{};

    void DelRoute(IPVariant_t, IPVariant_t) override{};

    void AddDefaultRouteViaInterface(std::string) override{};

    void DelDefaultRouteViaInterface(std::string) override{};

    void
    AddRouteViaInterface(NetworkInterface&, IPRange) override{};

    void
    DelRouteViaInterface(NetworkInterface&, IPRange) override{};

    std::vector<IPVariant_t> GetGatewaysNotOnInterface(std::string) override
    {
      return {};
    }
  };

  class NullPlatform : public Platform
  {
    NopRouteManager _routes;

   public:
    NullPlatform() : Platform{}, _routes{}
    {}

    virtual ~NullPlatform() = default;

    std::shared_ptr<NetworkInterface>
    ObtainInterface(InterfaceInfo, AbstractRouter*)
    {
      return std::static_pointer_cast<NetworkInterface>(std::make_shared<NullInterface>());
    }

    IRouteManager&
    RouteManager() override
    {
      return _routes;
    }
  };
}  // namespace llarp::vpn
