#include <llarp/ev.h>
#include <uv.h>
#include "mem.hpp"

struct llarp_ev_loop
{
  uv_loop_t _loop;

  static void * operator new(size_t sz)
  {
    return llarp_g_mem.alloc(sz, llarp::alignment<llarp_ev_loop>());
  }

  static void operator delete(void * ptr)
  {
    llarp_g_mem.free(ptr);
  }
  
  uv_loop_t * loop() { return &_loop; }
};

namespace llarp
{
  struct udp_listener
  {
    static void * operator new(size_t sz)
    {
      return llarp_g_mem.alloc(sz, alignment<udp_listener>());
    }

    static void operator delete(void * ptr)
    {
      llarp_g_mem.free(ptr);
    }
    
    uv_udp_t _handle;
    struct llarp_udp_listener * listener;
    
    void recvfrom(const struct sockaddr * addr, char * buff, ssize_t sz)
    {
      if(listener->recvfrom)
        listener->recvfrom(listener, addr, buff, sz);
    }

    /** called after closed */
    void closed()
    {
      if(listener->closed)
        listener->closed(listener);
      listener->impl = nullptr;
    }
    
    uv_udp_t * udp() { return &_handle; }
  };

  static void udp_alloc_cb(uv_handle_t * h, size_t sz, uv_buf_t * buf)
  {
    buf->base = static_cast<char *>(llarp_g_mem.alloc(sz, 1024));
    buf->len = sz;
  }

  static void udp_recv_cb(uv_udp_t* handle, ssize_t nread, const uv_buf_t* buf, const struct sockaddr* addr, unsigned flags)
  {
    udp_listener * l = static_cast<udp_listener *>(handle->data);
    l->recvfrom(addr, buf->base, nread);
    llarp_g_mem.free(buf->base);
  }

  static void udp_close_cb(uv_handle_t * handle)
  {
    udp_listener * l = static_cast<udp_listener *>(handle->data);
    l->closed();
    delete l;
  }
}


namespace llarp
{

  static void ev_handle_async_closed(uv_handle_t * handle)
  {
    struct llarp_ev_job * ev = static_cast<llarp_ev_job *>(handle->data);
    llarp_g_mem.free(ev);
    llarp_g_mem.free(handle);
  }
  
  static void ev_handle_async(uv_async_t * handle)
  {
    struct llarp_ev_job * ev = static_cast<llarp_ev_job *>(handle->data);
    ev->work(ev);
    uv_close((uv_handle_t *)handle, ev_handle_async_closed);
  }
}

extern "C" {
  void llarp_ev_loop_alloc(struct llarp_ev_loop ** ev)
  {
    *ev = new llarp_ev_loop;
    if (*ev)
    {
      uv_loop_init((*ev)->loop());
    }
  }
  
  void llarp_ev_loop_free(struct llarp_ev_loop ** ev)
  {
    if(*ev)
    {
      uv_loop_close((*ev)->loop());
      llarp_g_mem.free(*ev);
    }
    *ev = nullptr;
  }

  int llarp_ev_loop_run(struct llarp_ev_loop * ev)
  {
    return uv_run(ev->loop(), UV_RUN_DEFAULT);
  }
 
  int llarp_ev_add_udp_listener(struct llarp_ev_loop * ev, struct llarp_udp_listener * listener)
  {
    sockaddr_in6 addr;
    uv_ip6_addr(listener->host, listener->port, &addr);
    int ret = 0;
    llarp::udp_listener * l = new llarp::udp_listener;
    listener->impl = l;
    l->udp()->data = l;
    l->listener = listener;
    
    ret = uv_udp_init(ev->loop(), l->udp());
    if (ret == 0)
    {
      ret = uv_udp_bind(l->udp(), (const sockaddr *)&addr, 0);
      if (ret == 0)
      {
        ret = uv_udp_recv_start(l->udp(), llarp::udp_alloc_cb, llarp::udp_recv_cb);
      }
    }
    return ret;
  }

  int llarp_ev_close_udp_listener(struct llarp_udp_listener * listener)
  {
    int ret = -1;
    if(listener)
    {
      llarp::udp_listener * l = static_cast<llarp::udp_listener*>(listener->impl);
      if(l)
      {
        if(!uv_udp_recv_stop(l->udp()))
        {
          l->closed();
          delete l;
          ret = 0;
        }
      }
    }
    return ret;
  }

  void llarp_ev_loop_stop(struct llarp_ev_loop * loop)
  {
    uv_stop(loop->loop());
  }

  bool llarp_ev_async(struct llarp_ev_loop * loop, struct llarp_ev_job job)
  {
    struct llarp_ev_job * job_copy = static_cast<struct llarp_ev_job *>(llarp_g_mem.alloc(sizeof(struct llarp_ev_job), llarp::alignment<llarp_ev_job>()));
    job_copy->work = job.work;
    job_copy->loop = loop;
    job_copy->user = job.user;
    uv_async_t * async = static_cast<uv_async_t *>(llarp_g_mem.alloc(sizeof(uv_async_t), llarp::alignment<uv_async_t>()));
    async->data = job_copy;
    if(uv_async_init(loop->loop(), async, llarp::ev_handle_async) == 0 && uv_async_send(async))
      return true;
    else
    {
      llarp_g_mem.free(job_copy);
      llarp_g_mem.free(async);
      return false;
    }
  }
}
