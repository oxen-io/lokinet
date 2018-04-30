#include <llarp/ev.h>
#include "mem.hpp"

#ifdef __linux__
#include "ev_epoll.hpp"
#endif
#ifdef __freebsd__
#include "ev_kqueue.hpp"
#endif

extern "C" {
void llarp_ev_loop_alloc(struct llarp_ev_loop **ev) {
#ifdef __linux__
  *ev = new llarp_epoll_loop;
#endif
#ifdef __freebsd__
  *ev = new llarp_kqueue_loop;
#endif
}

void llarp_ev_loop_free(struct llarp_ev_loop **ev) {
  delete *ev;
  *ev = nullptr;
}

int llarp_ev_loop_run(struct llarp_ev_loop *ev) {
  return ev->run();
}

int llarp_ev_add_udp_listener(struct llarp_ev_loop *ev,
                              struct llarp_udp_listener *listener) {
  int ret = -1;
  return ret;
}

int llarp_ev_close_udp_listener(struct llarp_udp_listener *listener) {
  int ret = -1;
  return ret;
}

void llarp_ev_loop_stop(struct llarp_ev_loop *loop) { loop->stop(); }


}
