#ifndef LLARP_TIMER_H
#define LLARP_TIMER_H
#include <llarp/common.h>
#include <llarp/threadpool.h>
#ifdef __cplusplus
extern "C" {
#endif

/** called with userptr, original timeout, left */
typedef void (*llarp_timer_handler_func)(void *, uint64_t, uint64_t);

struct llarp_timeout_job
{
  uint64_t timeout;
  void *user;
  llarp_timer_handler_func handler;
};

struct llarp_timer_context;

struct llarp_timer_context *
llarp_init_timer();

uint32_t
llarp_timer_call_later(struct llarp_timer_context *t,
                       struct llarp_timeout_job job);

void
llarp_timer_cancel(struct llarp_timer_context *t, uint32_t id);

// cancel all
void
llarp_timer_stop(struct llarp_timer_context *t);

// blocking run timer and send events to thread pool
void
llarp_timer_run(struct llarp_timer_context *t, struct llarp_threadpool *pool);

void
llarp_free_timer(struct llarp_timer_context **t);

#ifdef __cplusplus
}
#endif
#endif
