#ifndef EV_KQUEUE_HPP
#define EV_KQUEUE_HPP
#include <llarp/buffer.h>
#include <llarp/net.h>

#if __FreeBSD__ || __OpenBSD__ || __NetBSD__ || (__APPLE__ && __MACH__)
// kqueue / kevent
#include <sys/event.h>
#include <sys/time.h>
#endif

// MacOS needs this
#ifndef SOCK_NONBLOCK
#include <fcntl.h>
#define SOCK_NONBLOCK O_NONBLOCK
#endif

// original upstream
#include <unistd.h>
#include <cstdio>
#include <llarp/net.hpp>
#include "ev.hpp"
#include "logger.hpp"

namespace llarp
{
  struct udp_listener : public ev_io
  {
    llarp_udp_io* udp;

    udp_listener(int fd, llarp_udp_io* u) : ev_io(fd), udp(u){};

    ~udp_listener()
    {
    }

    virtual int
    read(void* buf, size_t sz)
    {
      sockaddr_in6 src;
      socklen_t slen = sizeof(sockaddr_in6);
      sockaddr* addr = (sockaddr*)&src;
      ssize_t ret    = ::recvfrom(fd, buf, sz, 0, addr, &slen);
      if(ret == -1)
        return -1;
      udp->recvfrom(udp, addr, buf, ret);
      return 0;
    }

    virtual int
    sendto(const sockaddr* to, const void* data, size_t sz)
    {
      socklen_t slen;
      switch(to->sa_family)
      {
        case AF_INET:
          slen = sizeof(struct sockaddr_in);
          break;
        case AF_INET6:
          slen = sizeof(struct sockaddr_in6);
          break;
        default:
          return -1;
      }
      if(!fd)
      {
        printf("kqueue sendto fd empty\n");
        return -1;
      }
      ssize_t sent = ::sendto(fd, data, sz, 0, to, slen);
      if(sent == -1)
        perror("kqueue sendto()");
      return sent;
    }
  };

  struct tun : public ev_io
  {
    llarp_tun_io* t;
    device* tunif;
    tun(llarp_tun_io* tio)
        : ev_io(-1)
        , t(tio)
        , tunif(tuntap_init())

              {

              };

    int
    sendto(const sockaddr* to, const void* data, size_t sz)
    {
      return -1;
    }

    bool
    do_write(void* buf, size_t sz)
    {
      iovec vecs[2];
      // TODO: IPV6
      uint32_t t       = htonl(AF_INET);
      vecs[0].iov_base = &t;
      vecs[0].iov_len  = sizeof(t);
      vecs[1].iov_base = data;
      vecs[1].iov_len  = sz;
      return writev(fd, vecs, 2) != -1;
    }

    void
    flush_write()
    {
      if(t->before_write)
      {
        t->before_write(t);
        ev_io::flush_write();
      }
    }

    int
    read(void* buf, size_t sz)
    {
      ssize_t ret = tuntap_read(tunif, buf, sz);
      if(ret > 4 && t->recvpkt)
        t->recvpkt(t, ((byte_t*)buf) + 4, ret - 4);
      return ret;
    }

    bool
    setup()
    {
      llarp::LogDebug("set up tunif");
      if(tuntap_start(tunif, TUNTAP_MODE_TUNNEL, 0) == -1)
        return false;
      llarp::LogInfo("set ", tunif->if_name, " to use address ", t->ifaddr);
      if(tuntap_set_ip(tunif, t->ifaddr, t->ifaddr, t->netmask) == -1)
        return false;
      if(tuntap_up(tunif) == -1)
        return false;
      fd = tunif->tun_fd;
      return fd != -1;
    }

    ~tun()
    {
    }
  };

};  // namespace llarp

struct llarp_kqueue_loop : public llarp_ev_loop
{
  int kqueuefd;
  struct kevent change; /* event we want to monitor */

  llarp_kqueue_loop() : kqueuefd(-1)
  {
  }

  ~llarp_kqueue_loop()
  {
  }

  llarp::ev_io*
  create_tun(llarp_tun_io* tun)
  {
    llarp::tun* t = new llarp::tun(tun);
    if(t->setup())
      return t;
    delete t;
    return nullptr;
  }

  bool
  init()
  {
    if(kqueuefd == -1)
    {
      kqueuefd = kqueue();
    }
    return kqueuefd != -1;
  }

  bool
  running() const
  {
    return kqueuefd != -1;
  }

  int
  tick(int ms)
  {
    struct kevent events[1024];
    int result;
    timespec t;
    t.tv_sec  = 0;
    t.tv_nsec = ms * 1000UL;
    result    = kevent(kqueuefd, nullptr, 0, events, 1024, &t);
    // result: 0 is a timeout
    if(result > 0)
    {
      int idx = 0;
      while(idx < result)
      {
        llarp::ev_io* ev = static_cast< llarp::ev_io* >(events[idx].udata);
        ev->read(readbuf, sizeof(readbuf));
        ++idx;
      }
    }
    if(result != -1)
      tick_listeners();
    return result;
  }

  int
  run()
  {
    timespec t;
    t.tv_sec  = 0;
    t.tv_nsec = 1000UL * EV_TICK_INTERVAL;
    struct kevent events[1024];
    int result;
    do
    {
      result = kevent(kqueuefd, nullptr, 0, events, 1024, &t);
      // result: 0 is a timeout
      if(result > 0)
      {
        int idx = 0;
        while(idx < result)
        {
          llarp::ev_io* ev = static_cast< llarp::ev_io* >(events[idx].udata);
          if(ev && ev->fd)
          {
            // printf("reading_ev [%x] fd[%d]\n", ev, ev->fd);
            ev->read(readbuf, sizeof(readbuf));
          }
          else
          {
            llarp::LogWarn("kqueue event ", idx, " udata wasnt an ev_io");
          }
          ++idx;
        }
      }
      if(result != -1)
        tick_listeners();
    } while(result != -1);
    return result;
  }

  int
  udp_bind(const sockaddr* addr)
  {
    socklen_t slen;
    llarp::LogDebug("kqueue bind affam", addr->sa_family);
    switch(addr->sa_family)
    {
      case AF_INET:
        slen = sizeof(struct sockaddr_in);
        break;
      case AF_INET6:
        slen = sizeof(struct sockaddr_in6);
        break;
#ifdef AF_LINK
#endif
#ifdef AF_PACKET
      case AF_PACKET:
        slen = sizeof(struct sockaddr_ll);
        break;
#endif
      default:
        llarp::LogError("unsupported address family");
        return -1;
    }
    int fd = socket(addr->sa_family, SOCK_DGRAM, 0);
    if(fd == -1)
    {
      perror("socket()");
      return -1;
    }

    if(addr->sa_family == AF_INET6)
    {
      // enable dual stack explicitly
      int dual = 1;
      if(setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &dual, sizeof(dual)) == -1)
      {
        // failed
        perror("setsockopt()");
        close(fd);
        return -1;
      }
    }
    llarp::Addr a(*addr);
    llarp::LogInfo("bind to ", a);
    // FreeBSD handbook said to do this
    if(addr->sa_family == AF_INET && INADDR_ANY)
      a._addr4.sin_addr.s_addr = htonl(INADDR_ANY);

    if(bind(fd, addr, slen) == -1)
    {
      perror("bind()");
      close(fd);
      return -1;
    }

    return fd;
  }

  bool
  close_ev(llarp::ev_io* ev)
  {
    EV_SET(&change, ev->fd, EVFILT_READ, EV_DELETE, 0, 0, nullptr);
    return kevent(kqueuefd, &change, 1, nullptr, 0, nullptr) == -1;
  }

  llarp::ev_io*
  create_udp(llarp_udp_io* l, const sockaddr* src)
  {
    int fd = udp_bind(src);
    if(fd == -1)
      return nullptr;
    llarp::udp_listener* listener = new llarp::udp_listener(fd, l);
    udp_listeners.push_back(l);
    l->impl = listener;
    return listener;
  }

  bool
  add_ev(llarp::ev_io* ev, bool write)
  {
    if(write)
      EV_SET(&change, ev->fd, EVFILT_READ | EVFILT_WRITE, EV_ADD, 0, 0, ev);
    else
      EV_SET(&change, ev->fd, EVFILT_READ, EV_ADD, 0, 0, ev);
    if(kevent(kqueuefd, &change, 1, nullptr, 0, nullptr) == -1)
    {
      delete ev;
      return false;
    }
    return true;
  }

  bool
  udp_close(llarp_udp_io* l)
  {
    bool ret      = false;
    auto listener = static_cast< llarp::udp_listener* >(l->impl);
    if(listener)
    {
      // printf("Calling close_ev for [%x] fd[%d]\n", listener, listener->fd);
      ret     = close_ev(listener);
      l->impl = nullptr;
      delete listener;
      udp_listeners.remove(l);
    }
    return ret;
  }

  void
  stop()
  {
    if(kqueuefd != -1)
      ::close(kqueuefd);

    kqueuefd = -1;
  }
};

#endif
