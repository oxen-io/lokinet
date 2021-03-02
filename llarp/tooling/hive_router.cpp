#include <tooling/hive_router.hpp>

#include <tooling/router_hive.hpp>

namespace tooling
{
  HiveRouter::HiveRouter(
      llarp::EventLoop_ptr netloop,
      std::shared_ptr<llarp::Logic> logic,
      std::shared_ptr<llarp::vpn::Platform> plat,
      RouterHive* hive)
      : Router(netloop, logic, plat), m_hive(hive)
  {}

  bool
  HiveRouter::disableGossipingRC_TestingOnly()
  {
    return m_disableGossiping;
  }

  void
  HiveRouter::disableGossiping()
  {
    m_disableGossiping = true;
  }

  void
  HiveRouter::enableGossiping()
  {
    m_disableGossiping = false;
  }

  void
  HiveRouter::HandleRouterEvent(RouterEventPtr event) const
  {
    m_hive->NotifyEvent(std::move(event));
  }

}  // namespace tooling
