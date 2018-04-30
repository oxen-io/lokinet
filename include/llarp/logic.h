#ifndef LLARP_LOGIC_H
#define LLARP_LOGIC_H
#include <llarp/threadpool.h>
#include <llarp/timer.h>
#ifdef __cplusplus
extern "C" {
#endif

struct llarp_logic;

struct llarp_logic* llarp_init_logic();

void llarp_free_logic(struct llarp_logic** logic);

void llarp_logic_queue_job(struct llarp_logic* logic,
                           struct llarp_thread_job job);

uint32_t llarp_logic_call_later(struct llarp_logic* logic,
                                struct llarp_timeout_job job);
void llarp_logic_cancel_call(struct llarp_logic* logic, uint32_t id);

void llarp_logic_stop(struct llarp_logic* logic);

void llarp_logic_mainloop(struct llarp_logic* logic);

#ifdef __cplusplus
}
#endif
#endif
