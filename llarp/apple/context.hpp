#pragma once

#include <llarp.hpp>
#include "vpn_platform.hpp"
#include "route_manager.hpp"

namespace llarp::apple
{
  struct Context;

  /// make layers with platform specific quarks applied.
  std::unique_ptr<const layers::Layers>
  make_apple_layers(Context& ctx, Config conf);

  /// router with apple quarks applied.
  struct AppleRouter : public llarp::Router
  {
    AppleRouter(
        const std::shared_ptr<EventLoop>& loop, std::shared_ptr<VPNPlatform> plat, Context& ctx)
        : Router{loop, std::move(plat)}, _apple_ctx{ctx}

    {}

    ~AppleRouter() override = default;

    Context& _apple_ctx;

    std::unique_ptr<const layers::Layers>
    create_layers() override
    {
      return make_apple_layers(_apple_ctx, *m_Config);
    }
  };

  struct Context : public llarp::Context
  {
    std::shared_ptr<vpn::Platform>
    makeVPNPlatform() override
    {
      return std::make_shared<VPNPlatform>(*this, write_packet, route_callbacks, callback_context);
    }

    std::shared_ptr<AbstractRouter>
    makeRouter(const std::shared_ptr<EventLoop>& loop) override
    {
      auto ptr = std::make_shared<AppleRouter>(loop, makeVPNPlatform(), *this);
      return std::static_pointer_cast<AbstractRouter>(ptr);
    }

    // Callbacks that must be set for packet handling *before* calling Setup/Configure/Run; the main
    // point of these is to get passed through to VPNInterface, which will be called during setup,
    // after construction.
    packet_write_callback write_packet;
    on_readable_callback on_readable;
    llarp_route_callbacks route_callbacks{};
    void* callback_context = nullptr;
  };

  /// applies os traffic io quarks for apple platform
  class AppleOSTraffic_IO : public layers::platform::OSTraffic_IO_Base
  {
   public:
    using layers::platform::OSTraffic_IO_Base::OSTraffic_IO_Base;

    std::vector<layers::platform::OSTraffic>
    read_platform_traffic() override
    {
      auto vec = layers::platform::OSTraffic_IO_Base::read_platform_traffic();
      // we collected all reads from the past read cycle. queue reads again.
      if (auto ptr = std::dynamic_pointer_cast<llarp::apple::AppleVPNInterface>(_netif))
        ptr->on_readable();

      return vec;
    }
  };

  /// applies quarks for apple for the platform layer.
  class ApplePlatformLayer : public layers::platform::PlatformLayer
  {
   protected:
    std::unique_ptr<layers::platform::OSTraffic_IO_Base>
    make_io() const override
    {
      return std::unique_ptr<layers::platform::OSTraffic_IO_Base>{
          new AppleOSTraffic_IO{_router.loop()}};
    }

   public:
    using layers::platform::PlatformLayer::PlatformLayer;
  };

  std::unique_ptr<const layers::Layers>
  make_apple_layers(AbstractRouter& router, Config conf)
  {
    auto ptr = layers::make_layers(router, conf);
    ptr->platform.reset(new ApplePlatformLayer{router, *ptr->flow, conf.network});

    return ptr->freeze();
  }

}  // namespace llarp::apple
