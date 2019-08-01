#ifndef LLARP_THREADPOOL_H
#define LLARP_THREADPOOL_H

#include <util/queue.hpp>
#include <util/string_view.hpp>
#include <util/thread_pool.hpp>
#include <util/threading.hpp>

#include <absl/base/thread_annotations.h>
#include <memory>
#include <queue>

struct llarp_threadpool
{
  std::unique_ptr< llarp::thread::ThreadPool > impl;
  std::unique_ptr< llarp::thread::Queue< std::function< void(void) > > > jobs;
  const pid_t callingPID;

  llarp_threadpool(int workers, llarp::string_view name)
      : impl(std::make_unique< llarp::thread::ThreadPool >(workers,
                                                           workers * 128, name))
      , jobs(nullptr)
      , callingPID(0)
  {
  }

  llarp_threadpool()
      : jobs(new llarp::thread::Queue< std::function< void(void) > >(128))
      , callingPID(llarp::util::GetPid())
  {
    jobs->enable();
  }

  size_t
  size() const
  {
    if(jobs)
      return jobs->size();
    return 0;
  }

  bool
  QueueFunc(std::function< void(void) > f)
  {
    if(impl)
      return impl->tryAddJob(f);

    return jobs->tryPushBack(f) == llarp::thread::QueueReturn::Success;
  }
};

struct llarp_threadpool *
llarp_init_threadpool(int workers, const char *name);

/// for single process mode
struct llarp_threadpool *
llarp_init_same_process_threadpool();

void
llarp_free_threadpool(struct llarp_threadpool **tp);

typedef void (*llarp_thread_work_func)(void *);

/** job to be done in worker thread */
struct llarp_thread_job
{
  /** user data to pass to work function */
  void *user;
  /** called in threadpool worker thread */
  llarp_thread_work_func work;
#ifdef __cplusplus
  llarp_thread_job(void *u, llarp_thread_work_func w) : user(u), work(w)
  {
  }

  llarp_thread_job() : user(nullptr), work(nullptr)
  {
  }
#endif
};

/// for single process mode
void
llarp_threadpool_tick(struct llarp_threadpool *tp);

void
llarp_threadpool_queue_job(struct llarp_threadpool *tp,
                           struct llarp_thread_job j);

#ifdef __cplusplus

void
llarp_threadpool_queue_job(struct llarp_threadpool *tp,
                           std::function< void() > func);

#endif

void
llarp_threadpool_start(struct llarp_threadpool *tp);

void
llarp_threadpool_stop(struct llarp_threadpool *tp);
void
llarp_threadpool_join(struct llarp_threadpool *tp);

void
llarp_threadpool_wait(struct llarp_threadpool *tp);

#endif
