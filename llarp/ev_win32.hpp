#ifndef EV_WIN32_HPP
#define EV_WIN32_HPP
#include <llarp/buffer.h>
#include <llarp/net.h>
#include <windows.h>
#include <cstdio>
#include "ev.hpp"
#include "logger.hpp"
#include <llarp/net.hpp>

namespace llarp
{
  struct udp_listener : public ev_io
  {
    llarp_udp_io* udp;

	// we receive queued data in the OVERLAPPED data field,
	// much like the pipefds in the UNIX kqueue and loonix
	// epoll handles
	// 0 is the read port, 1 is the write port
    WSAOVERLAPPED portfds[2] = {0};

    udp_listener(SOCKET fd, llarp_udp_io* u) : ev_io(fd), udp(u){};

    ~udp_listener()
    {
    }

    virtual int
    read(void* buf, size_t sz)
    {
      sockaddr_in6 src;
      socklen_t slen = sizeof(sockaddr_in6);
      sockaddr* addr = (sockaddr*)&src;
      WSABUF wbuf    = {sz, static_cast< char* >(buf)};
      // WSARecvFrom
      int ret =
          ::WSARecvFrom(fd, &wbuf, sz, nullptr, 0, addr, &slen, &portfds[0], nullptr);
      if(ret == -1)
        return -1;
      udp->recvfrom(udp, addr, buf, ret);
      return 0;
    }

    virtual int
    sendto(const sockaddr* to, const void* data, size_t sz)
    {
      socklen_t slen;
      WSABUF wbuf = {sz, (char*)data};
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
      // WSASendTo
      ssize_t sent =
          ::WSASendTo(fd, &wbuf, sz, nullptr, 0, to, slen, &portfds[1], nullptr);
      if(sent == -1)
      {
        llarp::LogWarn(strerror(errno));
      }
      return sent;
    }
  };
};  // namespace llarp

struct llarp_win32_loop : public llarp_ev_loop
{
  HANDLE iocpfd;

  llarp_win32_loop() : iocpfd(INVALID_HANDLE_VALUE)
  {
    WSADATA wsockd;
    int err;
    // So, what I was told last time was that we can defer
    // loading winsock2 up to this point, as we reach this ctor
    // early on during daemon startup.
    err = ::WSAStartup(MAKEWORD(2, 2), &wsockd);
    if(err)
      perror("Failed to start Windows Sockets");
  }

  ~llarp_win32_loop()
  {
    if(iocpfd != INVALID_HANDLE_VALUE)
      ::CloseHandle(iocpfd);

    ::WSACleanup();
  }

  bool
  init()
  {
    if(iocpfd == INVALID_HANDLE_VALUE)
      iocpfd = ::CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0);

    if(iocpfd != INVALID_HANDLE_VALUE)
      return true;  // we don't have a socket to attach to this IOCP yet

    return false;
  }

  int
  tick(int ms)
  {
    return 0;
  }

  int
  run()
  {
    return 0;
  }

  SOCKET
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
      default:
        return INVALID_SOCKET;
    }
    SOCKET fd = ::WSASocket(addr->sa_family, SOCK_DGRAM, 0, nullptr, 0,
                            WSA_FLAG_OVERLAPPED);
    if(fd == INVALID_SOCKET)
    {
      perror("WSASocket()");
      return -1;
    }

    if(addr->sa_family == AF_INET6)
    {
      // enable dual stack explicitly
      int dual = 1;
      if(setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, (const char*)&dual, sizeof(dual))== -1)
      {
        // failed
        perror("setsockopt()");
        closesocket(fd);
        return INVALID_SOCKET;
      }
    }
    llarp::Addr a(*addr);
    llarp::LogDebug("bind to ", a);
    if(bind(fd, addr, slen) == -1)
    {
      perror("bind()");
      closesocket(fd);
      return INVALID_SOCKET;
    }
    return fd;
  }

  bool
  close_ev(llarp::ev_io* ev)
  {
    // On Windows, just close the socket to decrease the iocp refcount
    return closesocket(ev->fd) == 0;
  }

  bool
  udp_listen(llarp_udp_io* l, const sockaddr* src)
  {
    SOCKET fd = udp_bind(src);
    if(fd == INVALID_SOCKET)
      return false;
    llarp::udp_listener* listener = new llarp::udp_listener(fd, l);
    if(!::CreateIoCompletionPort(reinterpret_cast< HANDLE >(fd), iocpfd, 0, 0))
    {
      delete listener;
      return false;
    }
    l->impl = listener;
    udp_listeners.push_back(l);
    return true;
  }

  bool
  udp_close(llarp_udp_io* l)
  {
    bool ret = false;
    llarp::udp_listener* listener =
        static_cast< llarp::udp_listener* >(l->impl);
    if(listener)
    {
      ret = close_ev(listener);
      l->impl = nullptr;
      delete listener;
      udp_listeners.remove(l);
    }
    return ret;
  }

  void
  stop()
  {
    // do nothing, we dispose of the IOCP in destructor
  }
};

#endif
