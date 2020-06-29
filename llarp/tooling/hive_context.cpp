#include <tooling/hive_context.hpp>

#include <tooling/hive_router.hpp>

namespace tooling
{
  std::unique_ptr<llarp::AbstractRouter>
  HiveContext::makeRouter(
      std::shared_ptr<llarp::thread::ThreadPool> worker,
      llarp_ev_loop_ptr netloop,
      std::shared_ptr<llarp::Logic> logic)
  {
    return std::make_unique<HiveRouter>(worker, netloop, logic);
  }

}  // namespace tooling
