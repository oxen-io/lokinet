#ifndef LLARP_THREADPOOL_H
#define LLARP_THREADPOOL_H
#ifdef __cplusplus
extern "C" {
#endif

struct llarp_threadpool;

struct llarp_threadpool *llarp_init_threadpool(int workers, const char * name);
void llarp_free_threadpool(struct llarp_threadpool **tp);

typedef void (*llarp_thread_work_func)(void *);

/** job to be done in worker thread */
struct llarp_thread_job {
  /** user data to pass to work function */
  void *user;
  /** called in threadpool worker thread */
  llarp_thread_work_func work;
};

void llarp_threadpool_queue_job(struct llarp_threadpool *tp,
                                struct llarp_thread_job j);

void llarp_threadpool_stop(struct llarp_threadpool *tp);
void llarp_threadpool_join(struct llarp_threadpool *tp);

void llarp_threadpool_wait(struct llarp_threadpool *tp);

#ifdef __cplusplus
}
#endif

#endif
