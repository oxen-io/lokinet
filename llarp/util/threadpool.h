#ifndef LLARP_THREADPOOL_H
#define LLARP_THREADPOOL_H

#include <util/threading.hpp>
#include <util/threadpool.hpp>

#include <absl/base/thread_annotations.h>
#include <memory>
#include <queue>

struct llarp_threadpool
{
  std::unique_ptr< llarp::thread::ThreadPool > impl;

  mutable llarp::util::Mutex m_access;  // protects jobs
  std::queue< std::function< void(void) > > jobs GUARDED_BY(m_access);

  llarp_threadpool(int workers, const char *name)
      : impl(std::make_unique< llarp::thread::ThreadPool >(workers,
                                                           workers * 128))
  {
    (void)name;
  }

  llarp_threadpool()
  {
  }

  size_t
  size() const LOCKS_EXCLUDED(m_access)
  {
    absl::ReaderMutexLock l(&m_access);
    return jobs.size();
  }

  void
  QueueFunc(std::function< void(void) > f) LOCKS_EXCLUDED(m_access)
  {
    if(impl)
      impl->addJob(f);
    else
    {
      llarp::util::Lock lock(&m_access);
      jobs.emplace(f);
    }
  }
};

struct llarp_threadpool *
llarp_init_threadpool(int workers, const char *name);

/// for single process mode
struct llarp_threadpool *
llarp_init_same_process_threadpool();

typedef bool (*setup_net_func)(void *, bool);
typedef void (*run_main_func)(void *);

/// for network isolation
struct llarp_threadpool *
llarp_init_isolated_net_threadpool(const char *name, setup_net_func setupNet,
                                   run_main_func runMain, void *context);

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
void
llarp_threadpool_start(struct llarp_threadpool *tp);

void
llarp_threadpool_stop(struct llarp_threadpool *tp);
void
llarp_threadpool_join(struct llarp_threadpool *tp);

void
llarp_threadpool_wait(struct llarp_threadpool *tp);

#endif
