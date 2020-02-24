#include <util/logging/logger.hpp>
#include <util/time.hpp>
#include <util/thread/threadpool.h>
#include <util/thread/thread_pool.hpp>

#include <cstring>
#include <functional>
#include <queue>

struct llarp_threadpool *
llarp_init_threadpool(int workers, const char *name, size_t queueLength)
{
  if(workers <= 0)
    workers = 1;
  return new llarp_threadpool(workers, name, queueLength);
}

void
llarp_threadpool_join(struct llarp_threadpool *pool)
{
  llarp::LogDebug("threadpool join");
  if(pool->impl)
    pool->impl->stop();
  pool->impl.reset();
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
    pool->impl->disable();
}

bool
llarp_threadpool_queue_job(struct llarp_threadpool *pool,
                           struct llarp_thread_job job)
{
  return llarp_threadpool_queue_job(pool, std::bind(job.work, job.user));
}

bool
llarp_threadpool_queue_job(struct llarp_threadpool *pool,
                           std::function< void(void) > func)
{
  return pool->impl && pool->impl->addJob(func);
}

void
llarp_threadpool_tick(struct llarp_threadpool *pool)
{
  if(pool->impl)
  {
    pool->impl->drain();
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

size_t
llarp_threadpool::size() const
{
  return impl ? impl->capacity() : 0;
}

size_t
llarp_threadpool::pendingJobs() const
{
  return impl ? impl->jobCount() : 0;
}

size_t
llarp_threadpool::numThreads() const
{
  return impl ? impl->activeThreadCount() : 0;
}
