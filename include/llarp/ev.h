#ifndef LLARP_EV_H
#define LLARP_EV_H

#include <netinet/in.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/socket.h>

/**
 * ev.h
 *
 * event handler (cross platform high performance event system for IO)
 */

#ifdef __cplusplus
extern "C" {
#endif

// forward declare
struct llarp_threadpool;
struct llarp_logic;

struct llarp_ev_loop;

/// allocator
void
llarp_ev_loop_alloc(struct llarp_ev_loop **ev);

// deallocator
void
llarp_ev_loop_free(struct llarp_ev_loop **ev);

/// run main loop
int
llarp_ev_loop_run(struct llarp_ev_loop *ev);

void
llarp_ev_loop_run_single_process(struct llarp_ev_loop *ev,
                                 struct llarp_threadpool *tp,
                                 struct llarp_logic *logic);

/// stop event loop and wait for it to complete all jobs
void
llarp_ev_loop_stop(struct llarp_ev_loop *ev);

/// UDP handling configuration
struct llarp_udp_io
{
  void *user;
  void *impl;
  struct llarp_ev_loop *parent;
  /// called every event loop tick after reads
  void (*tick)(struct llarp_udp_io *);
  void (*recvfrom)(struct llarp_udp_io *, const struct sockaddr *, const void *,
                   ssize_t);
};

/// add UDP handler
int
llarp_ev_add_udp(struct llarp_ev_loop *ev, struct llarp_udp_io *udp,
                 const struct sockaddr *src);

/// schedule UDP packet
int
llarp_ev_udp_sendto(struct llarp_udp_io *udp, const struct sockaddr *to,
                    const void *data, size_t sz);

/// close UDP handler
int
llarp_ev_close_udp(struct llarp_udp_io *udp);

#ifdef __cplusplus
}
#endif
#endif
