
#ifndef LLARP_DHT_SEARCH_JOB_HPP
#define LLARP_DHT_SEARCH_JOB_HPP
#include <llarp/dht.h>
#include <llarp/time.h>
#include <functional>
#include <llarp/dht/key.hpp>
#include <llarp/service/IntroSet.hpp>
#include <set>
#include <vector>

namespace llarp
{
  namespace dht
  {
    /// TODO: this should be made into a templated type
    struct SearchJob
    {
      const static uint64_t JobTimeout = 30000;

      typedef std::function< bool(
          const std::vector< llarp::service::IntroSet >&) >
          IntroSetHookFunc;

      typedef std::function< void(const std::vector< RouterID >&) >
          FoundNearFunc;

      typedef std::function< void(void) > DoneFunc;

      SearchJob();

      /// for routers
      SearchJob(const Key_t& requester, uint64_t requesterTX,
                const Key_t& target, const std::set< Key_t >& excludes,
                llarp_router_lookup_job* job);
      /// for introsets
      SearchJob(const Key_t& requester, uint64_t requesterTX,
                const Key_t& target, const std::set< Key_t >& excludes,
                IntroSetHookFunc found, DoneFunc done);
      // for introsets via tag
      SearchJob(const Key_t& requester, uint64_t requseterTX,
                IntroSetHookFunc found, DoneFunc done);

      // for network exploration
      SearchJob(FoundNearFunc near, DoneFunc done);

      void
      FoundRouter(const llarp_rc* router) const;

      bool
      FoundIntros(
          const std::vector< llarp::service::IntroSet >& introset) const;

      void
      Timeout() const;

      bool
      IsExpired(llarp_time_t now) const;

      // only set if looking up router
      llarp_router_lookup_job* job = nullptr;
      IntroSetHookFunc foundIntroHook;
      // hook for exploritory router lookups
      FoundNearFunc foundNear;
      DoneFunc onDone;
      llarp_time_t started;
      Key_t requester;
      uint64_t requesterTX;
      Key_t target;
      std::set< Key_t > exclude;
    };
  }  // namespace dht
}  // namespace llarp
#endif
