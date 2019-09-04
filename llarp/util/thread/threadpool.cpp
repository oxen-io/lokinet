#include <util/logging/logger.hpp>
#include <util/time.hpp>
#include <util/thread/threadpool.h>
#include <util/thread/thread_pool.hpp>

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
  if(pool->jobs)
    pool->jobs->disable();
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
  return llarp_threadpool_queue_job(pool, (std::bind(job.work, job.user)));
}

void
llarp_threadpool_queue_job(struct llarp_threadpool *pool,
                           std::function< void() > func)
{
  if(pool->impl)
  {
    while(!pool->impl->tryAddJob(func))
    {
      std::this_thread::sleep_for(std::chrono::microseconds(1000));
    }
  }
  else
  {
    // single threaded mode
    while(pool->jobs->tryPushBack(func) != llarp::thread::QueueReturn::Success)
    {
      if(!pool->jobs->enabled())
        return;
      if(::getpid() == pool->callingPID)
        llarp_threadpool_tick(pool);
      else
        std::this_thread::sleep_for(std::chrono::microseconds(1000));
    }
  }
}

void
llarp_threadpool_tick(struct llarp_threadpool *pool)
{
  while(pool->size())
  {
    auto job = pool->jobs->tryPopFront();
    if(job)
    {
      (*job)();
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
