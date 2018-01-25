#include <sarp/ev.h>
#include <sarp/mem.h>
#include <uv.h>

struct sarp_ev_loop
{
  uv_loop_t _loop;

  uv_loop_t * loop() { return &_loop; }
};

namespace sarp
{
  struct udp_listener
  {
    uv_udp_t _handle;
    struct sarp_udp_listener * listener;
    
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
    buf->base = static_cast<char *>(sarp_g_mem.malloc(sz));
    buf->len = sz;
  }

  static void udp_recv_cb(uv_udp_t* handle, ssize_t nread, const uv_buf_t* buf, const struct sockaddr* addr, unsigned flags)
  {
    udp_listener * l = static_cast<udp_listener *>(handle->data);
    l->recvfrom(addr, buf->base, nread);
    sarp_g_mem.free(buf->base);
  }

  static void udp_close_cb(uv_handle_t * handle)
  {
    udp_listener * l = static_cast<udp_listener *>(handle->data);
    l->closed();
    sarp_g_mem.free(l);
  }
}


extern "C" {
  void sarp_ev_loop_alloc(struct sarp_ev_loop ** ev)
  {
    *ev = static_cast<sarp_ev_loop*>(sarp_g_mem.malloc(sizeof(struct sarp_ev_loop)));
    if (*ev)
    {
      uv_loop_init((*ev)->loop());
    }
  }
  
  void sarp_ev_loop_free(struct sarp_ev_loop ** ev)
  {
    if(*ev)
    {
      uv_loop_close((*ev)->loop());
      sarp_g_mem.free(*ev);
    }
    *ev = nullptr;
  }

  int sarp_ev_loop_run(struct sarp_ev_loop * ev)
  {
    return uv_run(ev->loop(), UV_RUN_DEFAULT);
  }
 
  int sarp_ev_add_udp_listener(struct sarp_ev_loop * ev, struct sarp_udp_listener * listener)
  {
    sockaddr_in6 addr;
    uv_ip6_addr(listener->host, listener->port, &addr);
    int ret = 0;
    sarp::udp_listener * l = static_cast<sarp::udp_listener *>(sarp_g_mem.malloc(sizeof(sarp::udp_listener)));
    listener->impl = l;
    l->udp()->data = l;
    l->listener = listener;
    
    ret = uv_udp_init(ev->loop(), l->udp());
    if (ret == 0)
    {
      ret = uv_udp_bind(l->udp(), (const sockaddr *)&addr, 0);
      if (ret == 0)
      {
        ret = uv_udp_recv_start(l->udp(), sarp::udp_alloc_cb, sarp::udp_recv_cb);
      }
    }
    return ret;
  }

  int sarp_ev_close_udp_listener(struct sarp_udp_listener * listener)
  {
    int ret = -1;
    if(listener)
    {
      sarp::udp_listener * l = static_cast<sarp::udp_listener*>(listener->impl);
      if(l)
      {
        if(!uv_udp_recv_stop(l->udp()))
        {
          l->closed();
          sarp_g_mem.free(l);
          ret = 0;
        }
      }
    }
    return ret;
  }
}
