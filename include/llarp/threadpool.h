#ifndef LLARP_THREADPOOL_H
#define LLARP_THREADPOOL_H

struct llarp_threadpool;

struct llarp_threadpool *
llarp_init_threadpool(int workers, const char *name);

/// for single process mode
struct llarp_threadpool *
llarp_init_same_process_threadpool();

/// for network isolation
struct llarp_threadpool *
llarp_init_isolated_net_threadpool(const char *name);

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
llarp_threadpool_stop(struct llarp_threadpool *tp);
void
llarp_threadpool_join(struct llarp_threadpool *tp);

void
llarp_threadpool_wait(struct llarp_threadpool *tp);

#endif
