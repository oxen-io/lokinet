#include <llarp/dht/search_job.hpp>
namespace llarp
{
  namespace dht
  {
    SearchJob::SearchJob()
    {
      started = 0;
      requester.Zero();
      target.Zero();
    }

    SearchJob::SearchJob(const Key_t &asker, uint64_t tx, const Key_t &key,
                         llarp_router_lookup_job *j,
                         const std::set< Key_t > &excludes)
        : job(j)
        , started(llarp_time_now_ms())
        , requester(asker)
        , requesterTX(tx)
        , target(key)
        , exclude(excludes)
    {
    }

    void
    SearchJob::Completed(const llarp_rc *router, bool timeout) const
    {
      if(job && job->hook)
      {
        if(router)
        {
          job->found = true;
          llarp_rc_copy(&job->result, router);
        }
        job->hook(job);
      }
    }

    bool
    SearchJob::IsExpired(llarp_time_t now) const
    {
      return now - started >= JobTimeout;
    }
  }
}