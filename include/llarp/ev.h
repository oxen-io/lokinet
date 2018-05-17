#ifndef LLARP_EV_H
#define LLARP_EV_H

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>

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
  struct sockaddr addr;
  void *user;
  void *impl;
  struct llarp_ev_loop * parent;
  void (*recvfrom)(struct llarp_udp_io *, const struct sockaddr *, void *,
                   ssize_t);
};

int llarp_ev_add_udp(struct llarp_ev_loop *ev, struct llarp_udp_io *udp);

int llarp_ev_udp_sendto(struct llarp_udp_io *udp, const struct sockaddr *to,
                        const void *data, size_t sz);

int llarp_ev_close_udp(struct llarp_udp_io *udp);

#ifdef __cplusplus
}
#endif
#endif
