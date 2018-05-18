#include <llarp/ev.h>
#include "mem.hpp"

#ifdef __linux__
#include "ev_epoll.hpp"
#endif
#ifdef __FreeBSD__
#include "ev_kqueue.hpp"
#endif

extern "C" {
void llarp_ev_loop_alloc(struct llarp_ev_loop **ev) {
#ifdef __linux__
  *ev = new llarp_epoll_loop;
#endif
#ifdef __FreeBSD__
  *ev = new llarp_kqueue_loop;
#endif
  (*ev)->init();
}

void llarp_ev_loop_free(struct llarp_ev_loop **ev) {
  delete *ev;
  *ev = nullptr;
}

int llarp_ev_loop_run(struct llarp_ev_loop *ev) { return ev->run(); }

int llarp_ev_add_udp(struct llarp_ev_loop *ev,
                              struct llarp_udp_io * udp) {
  udp->parent = ev;
  if(ev->udp_listen(udp)) return 0;
  return -1;
}

int llarp_ev_close_udp_close(struct llarp_udp_io * udp) {
  if(udp->parent->udp_close(udp)) return 0;
  return -1;
}

void llarp_ev_loop_stop(struct llarp_ev_loop *loop) { loop->stop(); }
}
