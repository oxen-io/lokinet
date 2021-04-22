#pragma once

#include <llarp.hpp>
#include "hive_router.hpp"

namespace tooling
{
  /// HiveContext is a subclass of llarp::Context which allows RouterHive to
  /// perform custom behavior which might be undesirable in production code.
  struct HiveContext : public llarp::Context
  {
    HiveContext(RouterHive* hive);

    std::shared_ptr<llarp::AbstractRouter>
    makeRouter(const llarp::EventLoop_ptr& loop) override;

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
