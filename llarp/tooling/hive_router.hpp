#pragma once

#include <router/router.hpp>

namespace tooling
{
  /// HiveRouter is a subclass of Router which overrides specific behavior in
  /// order to perform testing-related functions. It exists largely to prevent
  /// this behavior (which may often be "dangerous") from leaking into release
  /// code.
  struct HiveRouter : public llarp::Router
  {
    HiveRouter(
        std::shared_ptr<llarp::thread::ThreadPool> worker,
        llarp_ev_loop_ptr netloop,
        std::shared_ptr<llarp::Logic> logic);

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
  };

}  // namespace tooling
