#include "hive_context.hpp"

#include "hive_router.hpp"

namespace tooling
{
  HiveContext::HiveContext(RouterHive* hive) : m_hive(hive)
  {}

  std::shared_ptr<llarp::AbstractRouter>
  HiveContext::makeRouter(const llarp::EventLoop_ptr& loop)
  {
    return std::make_shared<HiveRouter>(loop, makeVPNPlatform(), m_hive);
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
