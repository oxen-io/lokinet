#include <llarp/ev.h>
#include "mem.hpp"

#ifdef __linux__
#include "ev_epoll.hpp"
#endif
#ifdef __freebsd__
#include "ev_kqueue.hpp"
#endif

#include <mutex>
#include <queue>

struct llarp_ev_caller {
  static void *operator new(size_t sz) {
    return llarp::Alloc<llarp_ev_caller>();
  }

  static void operator delete(void *ptr) { llarp_g_mem.free(ptr); }

  llarp_ev_caller(llarp_ev_loop *ev, llarp_ev_work_func func)
      : loop(ev), work(func) {
  }

  ~llarp_ev_caller() {}

  bool appendCall(void *user) {
    std::unique_lock<std::mutex> lock(access);
    pending.emplace_back(
        std::move(llarp_ev_async_call{loop, this, user, this->work}));
    return true;
  }

  bool appendManyCalls(void **users, size_t n) {
    std::unique_lock<std::mutex> lock(access);
    while (n--) {
      pending.emplace_back(
          std::move(llarp_ev_async_call{loop, this, *users, this->work}));
      users++;
    }
    return true;
  }

  void Call() {
    std::unique_lock<std::mutex> lock(access);
    auto sz = pending.size();
    while (sz > 0) {
      auto &front = pending.front();
      front.work(&front);
      pending.pop_front();
      --sz;
    }
  }

  std::mutex access;
  struct llarp_ev_loop *loop;
  std::deque<llarp_ev_async_call> pending;
  llarp_ev_work_func work;
};

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

struct llarp_ev_caller *llarp_ev_prepare_async(struct llarp_ev_loop *loop,
                                               llarp_ev_work_func work) {
  return new llarp_ev_caller(loop, work);
}

bool llarp_ev_call_async(struct llarp_ev_caller *caller, void *user) {
  return false;
}

bool llarp_ev_call_many_async(struct llarp_ev_caller *caller, void **users,
                              size_t n) {
  return false;
}

void llarp_ev_caller_stop(struct llarp_ev_caller *caller) {
  delete caller;
}
}
