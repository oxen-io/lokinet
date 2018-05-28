#include "threadpool.hpp"
#include <pthread.h>
#include <cstring>
#include "logger.hpp"

namespace llarp
{
  namespace thread
  {
    Pool::Pool(size_t workers, const char *name)
    {
      stop = false;
      while(workers--)
      {
        threads.emplace_back([this] {

          for(;;)
          {
            llarp_thread_job job;
            {
              lock_t lock(this->queue_mutex);
              this->condition.wait(
                  lock, [this] { return this->stop || !this->jobs.empty(); });
              if(this->stop && this->jobs.empty())
                return;
              job = std::move(this->jobs.front());
              this->jobs.pop_front();
            }
            // do work
            job.work(job.user);
          }
        });
      }
      if(name)
      {
        for(auto &t : threads)
          pthread_setname_np(t.native_handle(), name);
      }
    }

    void
    Pool::Stop()
    {
      {
        lock_t lock(queue_mutex);
        stop = true;
      }
      condition.notify_all();
    }

    void
    Pool::Join()
    {
      for(auto &t : threads)
        t.join();
      threads.clear();
      done.notify_all();
    }

    void
    Pool::QueueJob(const llarp_thread_job &job)
    {
      {
        lock_t lock(queue_mutex);

        // don't allow enqueueing after stopping the pool
        if(stop)
          return;

        jobs.emplace_back(job);
      }
      condition.notify_one();
    }

  }  // namespace thread
}  // namespace llarp

struct llarp_threadpool
{
  llarp::thread::Pool impl;

  llarp_threadpool(int workers, const char *name) : impl(workers, name)
  {
  }
};

extern "C" {

struct llarp_threadpool *
llarp_init_threadpool(int workers, const char *name)
{
  if(workers > 0)
    return new llarp_threadpool(workers, name);
  else
    return nullptr;
}

void
llarp_threadpool_join(struct llarp_threadpool *pool)
{
  llarp::Debug(__FILE__, "threadpool join");
  pool->impl.Join();
}

void
llarp_threadpool_start(struct llarp_threadpool *pool)
{ /** no op */
}

void
llarp_threadpool_stop(struct llarp_threadpool *pool)
{
  llarp::Debug(__FILE__, "threadpool stop");
  pool->impl.Stop();
}

void
llarp_threadpool_wait(struct llarp_threadpool *pool)
{
  std::mutex mtx;
  llarp::Debug(__FILE__, "threadpool wait");
  {
    std::unique_lock< std::mutex > lock(mtx);
    pool->impl.done.wait(lock);
  }
}

void
llarp_threadpool_queue_job(struct llarp_threadpool *pool,
                           struct llarp_thread_job job)
{
  pool->impl.QueueJob(job);
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
}
