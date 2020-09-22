#pragma once

#include <llarp.hpp>
#include <tooling/hive_router.hpp>

namespace tooling
{
  /// HiveContext is a subclass of llarp::Context which allows RouterHive to
  /// perform custom behavior which might be undesirable in production code.
  struct HiveContext : public llarp::Context
  {
    HiveContext(RouterHive* hive);

    std::unique_ptr<llarp::AbstractRouter>
    makeRouter(llarp_ev_loop_ptr netloop, std::shared_ptr<llarp::Logic> logic) override;

    /// Get this context's router as a HiveRouter.
    ///
    /// Returns nullptr if there is no router or throws an exception if the
    /// router is somehow not an instance of HiveRouter.
    HiveRouter*
    getRouterAsHiveRouter();

   protected:
    RouterHive* m_hive = nullptr;
  };

}  // namespace tooling
