
#ifndef LLARP_DHT_SEARCH_JOB_HPP
#define LLARP_DHT_SEARCH_JOB_HPP
#include <llarp/dht.h>
#include <llarp/time.h>
#include <functional>
#include <llarp/dht/key.hpp>
#include <llarp/service/IntroSet.hpp>
#include <set>

namespace llarp
{
  namespace dht
  {
    /// TODO: this should be made into a templated type
    struct SearchJob
    {
      const static uint64_t JobTimeout = 30000;

      typedef std::function< void(const llarp::service::IntroSet*) >
          IntroSetHookFunc;
      SearchJob();
      /// for routers
      SearchJob(const Key_t& requester, uint64_t requesterTX,
                const Key_t& target, const std::set< Key_t >& excludes,
                llarp_router_lookup_job* job);
      /// for introsets
      SearchJob(const Key_t& requester, uint64_t requesterTX,
                const Key_t& target, const std::set< Key_t >& excludes,
                IntroSetHookFunc found);

      void
      FoundRouter(const llarp_rc* router) const;

      void
      FoundIntro(const llarp::service::IntroSet* introset) const;

      void
      Timeout() const;

      bool
      IsExpired(llarp_time_t now) const;

      // only set if looking up router
      llarp_router_lookup_job* job = nullptr;
      IntroSetHookFunc foundIntroHook;
      llarp_time_t started;
      Key_t requester;
      uint64_t requesterTX;
      Key_t target;
      std::set< Key_t > exclude;
    };
  }  // namespace dht
}  // namespace llarp
#endif