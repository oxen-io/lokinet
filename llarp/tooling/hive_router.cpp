#include <tooling/hive_router.hpp>

#include <tooling/router_hive.hpp>

namespace tooling
{
  HiveRouter::HiveRouter(
      llarp_ev_loop_ptr netloop,
      std::shared_ptr<llarp::Logic> logic,
      RouterHive* hive)
      : Router(worker, netloop, logic), m_hive(hive)
  {
  }

  bool
  HiveRouter::disableGossipingRC_TestingOnly()
  {
    return m_disableGossiping;
  }

  void
  HiveRouter::disableGossiping()
  {
    m_disableGossiping = false;
  }

  void
  HiveRouter::enableGossiping()
  {
    m_disableGossiping = true;
  }

  void
  HiveRouter::HandleRouterEvent(RouterEventPtr event) const
  {
    m_hive->NotifyEvent(std::move(event));
  }

}  // namespace tooling
