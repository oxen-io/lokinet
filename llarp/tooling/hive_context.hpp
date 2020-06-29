#pragma once

#include <llarp.hpp>

namespace tooling
{
  /// HiveContext is a subclass of llarp::Context which allows RouterHive to
  /// perform custom behavior which might be undesirable in production code.
  struct HiveContext : public llarp::Context
  {
    std::unique_ptr<llarp::AbstractRouter>
    makeRouter(
        std::shared_ptr<llarp::thread::ThreadPool> worker,
        llarp_ev_loop_ptr netloop,
        std::shared_ptr<llarp::Logic> logic) override;
  };

}  // namespace tooling
