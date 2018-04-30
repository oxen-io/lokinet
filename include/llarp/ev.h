#ifndef LLARP_EV_H
#define LLARP_EV_H

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/socket.h>

#ifdef __cplusplus
extern "C" {
#endif

struct llarp_ev_loop;

void llarp_ev_loop_alloc(struct llarp_ev_loop **ev);
void llarp_ev_loop_free(struct llarp_ev_loop **ev);

int llarp_ev_loop_run(struct llarp_ev_loop *ev);
/** stop event loop and wait for it to complete all jobs */
void llarp_ev_loop_stop(struct llarp_ev_loop *ev);

struct llarp_udp_io {
  struct sockaddr_in6 *addr;
  void *user;
  void *impl;
  void (*recvfrom)(struct llarp_udp_io *, const struct sockaddr *, char *,
                   ssize_t);
};
  
int llarp_ev_add_udp(struct llarp_ev_loop *ev,
                              struct llarp_udp_io *udp);

int llarp_ev_udp_sendto(struct llarp_udp_io * udp, const struct sockaddr * to, const void * data, size_t sz);

int llarp_ev_close_udp(struct llarp_udp_io * udp);
  
struct llarp_ev_async_call;

typedef void (*llarp_ev_work_func)(struct llarp_ev_async_call *);

struct llarp_ev_caller;

struct llarp_ev_async_call {
  /** the loop this job belongs to */
  const struct llarp_ev_loop *loop;
  /** private implementation */
  const struct llarp_ev_caller *parent;
  /** user data */
  const void *user;
  /**
      work is called async when ready in the event loop thread
      must not free from inside this call as it is done elsewhere
   */
  const llarp_ev_work_func work;
};

struct llarp_ev_caller *llarp_ev_prepare_async(struct llarp_ev_loop *ev,
                                               llarp_ev_work_func func);

bool llarp_ev_call_async(struct llarp_ev_caller *c, void *user);

bool larp_ev_call_many_async(struct llarp_ev_caller *c, void **users, size_t n);

void llarp_ev_caller_stop(struct llarp_ev_caller *c);

#ifdef __cplusplus
}
#endif
#endif
