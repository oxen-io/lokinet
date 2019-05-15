#include <util/logger.hpp>
#include <util/threadpool.hpp>
#include <util/time.hpp>

#include <cstring>
#include <functional>
#include <queue>

struct llarp_threadpool *
llarp_init_threadpool(int workers, const char *name)
{
  if(workers <= 0)
    workers = 1;
  return new llarp_threadpool(workers, name);
}

struct llarp_threadpool *
llarp_init_same_process_threadpool()
{
  return new llarp_threadpool();
}

void
llarp_threadpool_join(struct llarp_threadpool *pool)
{
  llarp::LogDebug("threadpool join");
  if(pool->impl)
    pool->impl->drain();
}

void
llarp_threadpool_start(struct llarp_threadpool *pool)
{
  if(pool->impl)
    pool->impl->start();
}

void
llarp_threadpool_stop(struct llarp_threadpool *pool)
{
  llarp::LogDebug("threadpool stop");
  if(pool->impl)
    pool->impl->stop();
}

void
llarp_threadpool_wait(struct llarp_threadpool *pool)
{
  llarp::LogDebug("threadpool wait");
  if(pool->impl)
  {
    pool->impl->drain();
  }
}

void
llarp_threadpool_queue_job(struct llarp_threadpool *pool,
                           struct llarp_thread_job job)
{
  if(pool->impl)
  {
    while(!pool->impl->tryAddJob(std::bind(job.work, job.user)))
    {
      ::usleep(1000);
    }
  }
  else
  {
    // single threaded mode
    llarp::util::Lock lock(&pool->m_access);
    pool->jobs.emplace(std::bind(job.work, job.user));
  }
}

void
llarp_threadpool_tick(struct llarp_threadpool *pool)
{
  while(pool->size())
  {
    std::function< void(void) > job;
    {
      llarp::util::Lock lock(&pool->m_access);
      job = std::move(pool->jobs.front());
      pool->jobs.pop();
    }
    if(job)
    {
      job();
    }
  }
}

void
llarp_free_threadpool(struct llarp_threadpool **pool)
{
  if(*pool)
  {
    delete *pool;
  }
  *pool = nullptr;
}
