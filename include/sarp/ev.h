#ifndef SARP_EV_H
#define SARP_EV_H

#include <stdlib.h>
#include <stdint.h>
#include <sys/socket.h>

#ifdef __cplusplus
extern "C" {
#endif

  struct sarp_ev_loop;

  void sarp_ev_loop_alloc(struct sarp_ev_loop ** ev);
  void sarp_ev_loop_free(struct sarp_ev_loop ** ev);

  int sarp_ev_loop_run(struct sarp_ev_loop * ev);

  struct sarp_udp_listener
  {
    char * host;
    uint16_t port;
    void * user;
    void * impl;
    void (*recvfrom)(struct sarp_udp_listener *, const struct sockaddr *,  char *, ssize_t);
    void (*closed)(struct sarp_udp_listener *);
  };
  
  int sarp_ev_add_udp_listener(struct sarp_ev_loop * ev, struct sarp_udp_listener * listener);

  int sarp_ev_close_udp_listener(struct sarp_udp_listener * listener);
  
#ifdef __cplusplus
}
#endif
#endif
