#include <tooling/hive_router.hpp>

namespace tooling
{
  HiveRouter::HiveRouter(
      std::shared_ptr<llarp::thread::ThreadPool> worker,
      llarp_ev_loop_ptr netloop,
      std::shared_ptr<llarp::Logic> logic)
      : Router(worker, netloop, logic)
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

}  // namespace tooling
