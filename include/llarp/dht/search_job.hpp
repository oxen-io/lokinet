
#ifndef LLARP_DHT_SEARCH_JOB_HPP
#define LLARP_DHT_SEARCH_JOB_HPP
#include <llarp/dht.h>
#include <llarp/time.h>
#include <llarp/dht/key.hpp>

#include <set>

namespace llarp
{
  namespace dht
  {
    struct SearchJob
    {
      const static uint64_t JobTimeout = 30000;

      SearchJob();

      SearchJob(const Key_t& requester, uint64_t requesterTX,
                const Key_t& target, llarp_router_lookup_job* job,
                const std::set< Key_t >& excludes);

      void
      Completed(const llarp_rc* router, bool timeout = false) const;

      bool
      IsExpired(llarp_time_t now) const;

      llarp_router_lookup_job* job = nullptr;
      llarp_time_t started;
      Key_t requester;
      uint64_t requesterTX;
      Key_t target;
      std::set< Key_t > exclude;
    };
  }
}
#endif