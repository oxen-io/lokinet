#ifndef LLARP_THREADPOOL_H
#define LLARP_THREADPOOL_H

#include <util/thread/queue.hpp>
#include <util/thread/thread_pool.hpp>
#include <util/thread/threading.hpp>
#include <util/thread/annotations.hpp>
#include <util/types.hpp>

#include <memory>
#include <queue>
#include <string_view>

struct llarp_threadpool;

#ifdef __cplusplus
struct llarp_threadpool
{
  std::unique_ptr<llarp::thread::ThreadPool> impl;

  llarp_threadpool(int workers, std::string_view name, size_t queueLength = size_t{1024 * 8})
      : impl(std::make_unique<llarp::thread::ThreadPool>(
          workers, std::max(queueLength, size_t{32}), name))
  {
  }

  size_t
  size() const;

  size_t
  pendingJobs() const;

  size_t
  numThreads() const;

  /// see if this thread is full given lookahead amount
  bool
  LooksFull(size_t lookahead) const
  {
    return (pendingJobs() + lookahead) >= size();
  }
};
#endif

struct llarp_threadpool*
llarp_init_threadpool(int workers, const char* name, size_t queueLength);

void
llarp_free_threadpool(struct llarp_threadpool** tp);

using llarp_thread_work_func = void (*)(void*);

/** job to be done in worker thread */
struct llarp_thread_job
{
#ifdef __cplusplus
  /** user data to pass to work function */
  void* user{nullptr};
  /** called in threadpool worker thread */
  llarp_thread_work_func work{nullptr};

  llarp_thread_job(void* u, llarp_thread_work_func w) : user(u), work(w)
  {
  }

  llarp_thread_job() = default;
#else
  void* user;
  llarp_thread_work_func work;
#endif
};

void
llarp_threadpool_tick(struct llarp_threadpool* tp);

bool
llarp_threadpool_queue_job(struct llarp_threadpool* tp, struct llarp_thread_job j);

#ifdef __cplusplus

bool
llarp_threadpool_queue_job(struct llarp_threadpool* tp, std::function<void(void)> func);

#endif

void
llarp_threadpool_start(struct llarp_threadpool* tp);
void
llarp_threadpool_stop(struct llarp_threadpool* tp);

#endif
