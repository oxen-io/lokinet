#ifndef EV_EPOLL_HPP
#define EV_EPOLL_HPP
#include "ev.hpp"
#include <unistd.h>
#include <sys/epoll.h>
#include <netinet/in.h>


namespace llarp
{
  struct udp_listener : public ev_io
  {
    llarp_udp_io * udp;
    
    udp_listener(int fd, llarp_udp_io * u) :
      ev_io(fd),
      udp(u) {};

    ~udp_listener() {}
    
    virtual int read()
    {
      sockaddr src;
      socklen_t slen;
      int ret = ::recvfrom(fd, buff, sizeof(buff), 0, &src, &slen);
      if (ret == -1) return -1;
      udp->recvfrom(udp, &src, buff, ret);
      return 0;
    }

    virtual int sendto(const sockaddr * to, const void * data, size_t sz)
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
      return ::sendto(fd, data, sz, 0, to, slen);
    }
  };
};

struct llarp_epoll_loop : public llarp_ev_loop
{

  int epollfd;
  
  llarp_epoll_loop() : epollfd(-1)
  {
    
  }

  ~llarp_epoll_loop()
  {
  }

  bool init()
  {
    if(epollfd == -1)
      epollfd = epoll_create1(EPOLL_CLOEXEC);
    return epollfd != -1;
  }

  int run()
  {
    epoll_event events[1024];
    int result;
    do
    {
      result = epoll_wait(epollfd, events, 1024, 0);
      if(result > 0)
      {
        int idx = 0;
        while(idx < result)
        {
          llarp::ev_io * ev = static_cast<llarp::ev_io*>(events[idx].data.ptr);
          if(events[idx].events & EPOLLIN)
          {
            if ( ev->read() == -1)
            {
              epoll_ctl(epollfd, EPOLL_CTL_DEL, ev->fd, nullptr);
            }
          }
          ++idx;
        }
      }
    }
    while(result != -1);
    return result;
  }

  int udp_bind(const sockaddr * addr)
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
    default:
      return -1;
    }
    int fd = socket(addr->sa_family, SOCK_DGRAM, 0);
    if (fd == -1) return -1;
    if(bind(fd, addr, slen) == -1)
    {
      close(fd);
      return -1;
    }
    return fd;
  }

  bool udp_listen(llarp_udp_io * l)
  {
    int fd = udp_bind((sockaddr*)l->addr);
    if (fd == -1) return false;
    llarp::udp_listener * listener = new llarp::udp_listener(fd, l);
    epoll_event ev;
    ev.data.ptr = listener;
    ev.events = EPOLLIN;
    if(epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &ev) == -1)
    {
      delete listener;
      return false;
    }
    return true;
  }
  
  void stop()
  {
    if(epollfd != -1)
      ::close(epollfd);
    
    epollfd = -1;
  }
};

#endif
