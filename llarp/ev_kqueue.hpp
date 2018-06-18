#ifndef EV_KQUEUE_HPP
#define EV_KQUEUE_HPP
#include <llarp/buffer.h>
#include <llarp/net.h>

#if __FreeBSD__
// kqueue / kevent
#include <sys/event.h>
#include <sys/time.h>
#endif

#if(__APPLE__ && __MACH__)
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
#include "ev.hpp"
#include "logger.hpp"
#include "net.hpp"

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
      ssize_t ret        = ::recvfrom(fd, buf, sz, 0, addr, &slen);
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
      ssize_t sent = ::sendto(fd, data, sz, 0, to, slen);
      if(sent == -1)
        perror("kqueue sendto()");
      return sent;
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

  bool
  init()
  {
    if(kqueuefd == -1)
    {
      kqueuefd = kqueue();
    }
    return kqueuefd != -1;
  }

  int
  tick(int ms)
  {
    (void)ms;
    struct kevent events[1024];
    int result;
    byte_t readbuf[2048];
    result = kevent(kqueuefd, NULL, 0, events, 1024, NULL);
    // result: 0 is a timeout
    if(result > 0)
    {
      int idx = 0;
      while(idx < result)
      {
        llarp::ev_io* ev = static_cast< llarp::ev_io* >(events[idx].udata);
        if(ev->read(readbuf, sizeof(readbuf)) == -1)
        {
          llarp::Info("close ev");
          close_ev(ev);
          delete ev;
        }
        ++idx;
      }
    }
    return result;
  }

  int
  run()
  {
    struct kevent events[1024];
    int result;
    byte_t readbuf[2048];
    do
    {
      result = kevent(kqueuefd, NULL, 0, events, 1024, NULL);
      // result: 0 is a timeout
      if(result > 0)
      {
        int idx = 0;
        while(idx < result)
        {
          llarp::ev_io* ev = static_cast< llarp::ev_io* >(events[idx].udata);
          if(ev->read(readbuf, sizeof(readbuf)) == -1)
          {
            llarp::Info("close ev");
            close_ev(ev);
            delete ev;
          }
          ++idx;
        }
      }
    } while(result != -1);
    return result;
  }

  int
  udp_bind(const sockaddr* addr)
  {
    socklen_t slen;
    llarp::Debug("kqueue bind affam", addr->sa_family);
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
        llarp::Error("unsupported address family");
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
    llarp::Info("bind to ", a);
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
    EV_SET(&change, ev->fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
    return kevent(kqueuefd, &change, 1, NULL, 0, NULL) == -1;
  }

  bool
  udp_listen(llarp_udp_io* l, const sockaddr* src)
  {
    int fd = udp_bind(src);
    if(fd == -1)
      return false;
    llarp::udp_listener* listener = new llarp::udp_listener(fd, l);

    EV_SET(&change, fd, EVFILT_READ, EV_ADD, 0, 0, listener);
    if(kevent(kqueuefd, &change, 1, NULL, 0, NULL) == -1)
    {
      delete listener;
      return false;
    }

    l->impl = listener;
    return true;
  }

  bool
  udp_close(llarp_udp_io* l)
  {
    bool ret      = false;
    auto listener = static_cast< llarp::udp_listener* >(l->impl);
    if(listener)
    {
      ret = close_ev(listener);
      delete listener;
      l->impl = nullptr;
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
