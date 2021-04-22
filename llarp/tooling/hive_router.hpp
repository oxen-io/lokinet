#pragma once

#include <llarp/router/router.hpp>

namespace tooling
{
  /// HiveRouter is a subclass of Router which overrides specific behavior in
  /// order to perform testing-related functions. It exists largely to prevent
  /// this behavior (which may often be "dangerous") from leaking into release
  /// code.
  struct HiveRouter : public llarp::Router
  {
    explicit HiveRouter(
        llarp::EventLoop_ptr loop,
        std::shared_ptr<llarp::vpn::Platform> vpnPlatform,
        RouterHive* hive);

    virtual ~HiveRouter() = default;

    /// Override logic to prevent base Router class from gossiping its RC.
    virtual bool
    disableGossipingRC_TestingOnly() override;

    void
    disableGossiping();

    void
    enableGossiping();

   protected:
    bool m_disableGossiping = false;
    RouterHive* m_hive = nullptr;

    virtual void
    HandleRouterEvent(RouterEventPtr event) const override;
  };

}  // namespace tooling
