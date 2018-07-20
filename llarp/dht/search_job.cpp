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
                         const std::set< Key_t > &excludes,
                         llarp_router_lookup_job *j)
        : job(j)
        , started(llarp_time_now_ms())
        , requester(asker)
        , requesterTX(tx)
        , target(key)
        , exclude(excludes)
    {
    }

    SearchJob::SearchJob(const Key_t &asker, uint64_t tx, const Key_t &key,
                         const std::set< Key_t > &excludes,
                         IntroSetHookFunc foundIntroset)
        : foundIntroHook(foundIntroset)
        , started(llarp_time_now_ms())
        , requester(asker)
        , requesterTX(tx)
        , target(key)
        , exclude(excludes)
    {
    }

    SearchJob::SearchJob(const Key_t &asker, uint64_t tx,
                         IntroSetHookFunc found)
        : foundIntroHook(found)
        , started(llarp_time_now_ms())
        , requester(asker)
        , requesterTX(tx)
    {
      target.Zero();
    }

    void
    SearchJob::FoundIntros(
        const std::vector< llarp::service::IntroSet > &introsets) const
    {
      if(foundIntroHook)
        foundIntroHook(introsets);
    }

    void
    SearchJob::FoundRouter(const llarp_rc *router) const
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

    void
    SearchJob::Timeout() const
    {
      if(job)
      {
        job->found = false;
        job->hook(job);
      }
      else if(foundIntroHook)
      {
        foundIntroHook({});
      }
    }
  }  // namespace dht
}  // namespace llarp