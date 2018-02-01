#include <llarp/ev.h>
#include <uv.h>
#include "mem.hpp"

#include <mutex>
#include <queue>

struct llarp_ev_caller {
  static void *operator new(size_t sz) {
    return llarp::Alloc<llarp_ev_caller>();
  }

  static void operator delete(void *ptr) { llarp_g_mem.free(ptr); }

  llarp_ev_caller(llarp_ev_loop *ev, llarp_ev_work_func func)
      : loop(ev), work(func) {
    async.data = this;
  }

  ~llarp_ev_caller() {}

  bool appendCall(void *user) {
    std::unique_lock<std::mutex> lock(access);
    bool should = pending.size() == 0;
    llarp_ev_async_call *call =
        new llarp_ev_async_call{loop, this, user, this->work};
    pending.push(call);
    return should;
  }

  void Call() {
    std::unique_lock<std::mutex> lock(access);
    while (pending.size() > 0) {
      auto &front = pending.front();
      front->work(front);
      pending.pop();
    }
  }

  std::mutex access;
  struct llarp_ev_loop *loop;
  uv_async_t async;
  std::queue<llarp_ev_async_call *> pending;
  llarp_ev_work_func work;
};

struct llarp_ev_loop {
  uv_loop_t _loop;

  static void *operator new(size_t sz) {
    return llarp_g_mem.alloc(sz, llarp::alignment<llarp_ev_loop>());
  }

  static void operator delete(void *ptr) { llarp_g_mem.free(ptr); }

  uv_loop_t *loop() { return &_loop; }
};

namespace llarp {
struct udp_listener {
  static void *operator new(size_t sz) {
    return llarp_g_mem.alloc(sz, alignment<udp_listener>());
  }

  static void operator delete(void *ptr) { llarp_g_mem.free(ptr); }

  uv_udp_t _handle;
  struct llarp_udp_listener *listener;

  void recvfrom(const struct sockaddr *addr, char *buff, ssize_t sz) {
    if (listener->recvfrom) listener->recvfrom(listener, addr, buff, sz);
  }

  /** called after closed */
  void closed() {
    if (listener->closed) listener->closed(listener);
    listener->impl = nullptr;
  }

  uv_udp_t *udp() { return &_handle; }
};

static void udp_alloc_cb(uv_handle_t *h, size_t sz, uv_buf_t *buf) {
  buf->base = static_cast<char *>(llarp_g_mem.alloc(sz, 1024));
  buf->len = sz;
}

static void udp_recv_cb(uv_udp_t *handle, ssize_t nread, const uv_buf_t *buf,
                        const struct sockaddr *addr, unsigned flags) {
  udp_listener *l = static_cast<udp_listener *>(handle->data);
  l->recvfrom(addr, buf->base, nread);
  llarp_g_mem.free(buf->base);
}

static void udp_close_cb(uv_handle_t *handle) {
  udp_listener *l = static_cast<udp_listener *>(handle->data);
  l->closed();
  delete l;
}
}  // namespace llarp

namespace llarp {

static void ev_caller_async_closed(uv_handle_t *handle) {
  llarp_ev_caller *caller = static_cast<llarp_ev_caller *>(handle->data);
  delete caller;
}

static void ev_handle_async_call(uv_async_t *handle) {
  llarp_ev_caller *caller = static_cast<llarp_ev_caller *>(handle->data);
  caller->Call();
}
}  // namespace llarp

extern "C" {
void llarp_ev_loop_alloc(struct llarp_ev_loop **ev) {
  *ev = new llarp_ev_loop;
  if (*ev) {
    uv_loop_init((*ev)->loop());
  }
}

void llarp_ev_loop_free(struct llarp_ev_loop **ev) {
  if (*ev) {
    uv_loop_close((*ev)->loop());
    llarp_g_mem.free(*ev);
  }
  *ev = nullptr;
}

int llarp_ev_loop_run(struct llarp_ev_loop *ev) {
  return uv_run(ev->loop(), UV_RUN_DEFAULT);
}

int llarp_ev_add_udp_listener(struct llarp_ev_loop *ev,
                              struct llarp_udp_listener *listener) {
  sockaddr_in6 addr;
  uv_ip6_addr(listener->host, listener->port, &addr);
  int ret = 0;
  llarp::udp_listener *l = new llarp::udp_listener;
  listener->impl = l;
  l->udp()->data = l;
  l->listener = listener;

  ret = uv_udp_init(ev->loop(), l->udp());
  if (ret == 0) {
    ret = uv_udp_bind(l->udp(), (const sockaddr *)&addr, 0);
    if (ret == 0) {
      ret =
          uv_udp_recv_start(l->udp(), llarp::udp_alloc_cb, llarp::udp_recv_cb);
    }
  }
  return ret;
}

int llarp_ev_close_udp_listener(struct llarp_udp_listener *listener) {
  int ret = -1;
  if (listener) {
    llarp::udp_listener *l = static_cast<llarp::udp_listener *>(listener->impl);
    if (l) {
      if (!uv_udp_recv_stop(l->udp())) {
        l->closed();
        delete l;
        ret = 0;
      }
    }
  }
  return ret;
}

void llarp_ev_loop_stop(struct llarp_ev_loop *loop) { uv_stop(loop->loop()); }

struct llarp_ev_caller *llarp_ev_prepare_async(struct llarp_ev_loop *loop,
                                               llarp_ev_work_func work) {
  llarp_ev_caller *caller = new llarp_ev_caller(loop, work);
  if (uv_async_init(loop->loop(), &caller->async,
                    llarp::ev_handle_async_call) == 0)
    return caller;
  else {
    delete caller;
    return nullptr;
  }
}

bool llarp_ev_call_async(struct llarp_ev_caller *caller, void *user) {
  if (caller->appendCall(user))
    return uv_async_send(&caller->async) == 0;
  else
    return true;
}

void llarp_ev_caller_stop(struct llarp_ev_caller *caller) {
  uv_close((uv_handle_t *)&caller->async, llarp::ev_caller_async_closed);
}
}
