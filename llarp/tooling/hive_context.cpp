#include <tooling/hive_context.hpp>

#include <tooling/hive_router.hpp>

namespace tooling
{
  HiveContext::HiveContext(RouterHive* hive) : m_hive(hive)
  {
  }

  std::unique_ptr<llarp::AbstractRouter>
  HiveContext::makeRouter(llarp_ev_loop_ptr netloop, std::shared_ptr<llarp::Logic> logic)
  {
    return std::make_unique<HiveRouter>(netloop, logic, m_hive);
  }

  HiveRouter*
  HiveContext::getRouterAsHiveRouter()
  {
    if (not router)
      return nullptr;

    HiveRouter* hiveRouter = dynamic_cast<HiveRouter*>(router.get());

    if (hiveRouter == nullptr)
      throw std::runtime_error("HiveContext has a router not of type HiveRouter");

    return hiveRouter;
  }

}  // namespace tooling
