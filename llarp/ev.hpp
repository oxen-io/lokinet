#ifndef LLARP_EV_HPP
#define LLARP_EV_HPP
#include <llarp/ev.h>

#ifndef _MSC_VER
#include <unistd.h>
#endif
#include <list>

namespace llarp
{
  struct ev_io
  {
#ifndef _WIN32
    int fd;
    ev_io(int f) : fd(f){};
#else
    SOCKET fd;
    ev_io(SOCKET f) : fd(f){};
#endif
    virtual int
    read(void* buf, size_t sz) = 0;

    virtual int
    sendto(const sockaddr* dst, const void* data, size_t sz) = 0;

    virtual ~ev_io()
    {
#ifndef _WIN32
      ::close(fd);
#else
      closesocket(fd);
#endif
    };
  };
};  // namespace llarp

struct llarp_ev_loop
{
  virtual bool
  init() = 0;
  virtual int
  run() = 0;

  virtual int
  tick(int ms) = 0;

  virtual void
  stop() = 0;

  virtual bool
  udp_listen(llarp_udp_io* l, const sockaddr* src) = 0;
  virtual bool
  udp_close(llarp_udp_io* l) = 0;
  virtual bool
  close_ev(llarp::ev_io* ev) = 0;

  virtual bool
  running() const = 0;

  virtual ~llarp_ev_loop(){};

  std::list< llarp_udp_io* > udp_listeners;
};

#endif
