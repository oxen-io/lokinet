#ifndef EV_EPOLL_HPP
#define EV_EPOLL_HPP
#include <llarp/buffer.h>
#include <llarp/net.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <cstdio>
#include "ev.hpp"
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
      sockaddr src;
      socklen_t slen;
      int ret = ::recvfrom(fd, buf, sz, 0, &src, &slen);
      if(ret == -1)
        return -1;
      udp->recvfrom(udp, &src, buf, ret);
      return 0;
    }

    virtual int
    sendto(const sockaddr* to, const void* data, size_t sz)
    {
      socklen_t slen;
      switch(to->sa_family)
      {
        case AF_INET:
          printf("af_inet\n");
          slen = sizeof(struct sockaddr_in);
          break;
        case AF_INET6:
          printf("af_inet6\n");
          slen = sizeof(struct sockaddr_in6);
          break;
        default:
          return -1;
      }
      printf("send %ld via %d\n", sz, fd);
      ssize_t sent = ::sendto(fd, data, sz, SOCK_NONBLOCK, to, slen);
      if(sent == -1)
        perror("sendto()");
      return sent;
    }
  };
};  // namespace llarp

struct llarp_epoll_loop : public llarp_ev_loop
{
  int epollfd;

  llarp_epoll_loop() : epollfd(-1)
  {
  }

  ~llarp_epoll_loop()
  {
  }

  bool
  init()
  {
    if(epollfd == -1)
      epollfd = epoll_create1(EPOLL_CLOEXEC);
    return epollfd != -1;
  }

  int
  run()
  {
    epoll_event events[1024];
    int result;
    byte_t readbuf[2048];
    do
    {
      result = epoll_wait(epollfd, events, 1024, -1);
      if(result > 0)
      {
        printf("epoll %d\n", result);
        int idx = 0;
        while(idx < result)
        {
          llarp::ev_io* ev = static_cast< llarp::ev_io* >(events[idx].data.ptr);
          if(events[idx].events & EPOLLIN)
          {
            if(ev->read(readbuf, sizeof(readbuf)) == -1)
            {
              printf("close ev\n");
              close_ev(ev);
              delete ev;
            }
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
    switch(addr->sa_family)
    {
      case AF_INET:
        slen = sizeof(struct sockaddr_in);
        break;
      case AF_INET6:
        slen = sizeof(struct sockaddr_in6);
        break;
      case AF_PACKET:
        slen = sizeof(struct sockaddr_ll);
        break;
      default:
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
    printf("binding to %s\n", a.to_string().c_str());
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
    return epoll_ctl(epollfd, EPOLL_CTL_DEL, ev->fd, nullptr) != -1;
  }

  bool
  udp_listen(llarp_udp_io* l, const sockaddr* src)
  {
    int fd = udp_bind(src);
    if(fd == -1)
      return false;
    llarp::udp_listener* listener = new llarp::udp_listener(fd, l);
    epoll_event ev;
    ev.data.ptr = listener;
    ev.events   = EPOLLIN;
    if(epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &ev) == -1)
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
    if(epollfd != -1)
      ::close(epollfd);

    epollfd = -1;
  }
};

#endif
