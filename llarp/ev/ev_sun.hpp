#ifndef EV_POLL_HPP
#define EV_POLL_HPP

#include <ev/ev.hpp>
#include <net/net.h>
#include <net/net.hpp>
#include <util/buffer.hpp>
#include <util/buffer.hpp>
#include <util/logger.hpp>
#include <util/mem.hpp>

#include <cassert>
#include <cstdio>
#include <fcntl.h>
#include <signal.h>
#include "upoll_sun.h"
#include <sys/un.h>
#include <tuntap.h>
#include <unistd.h>

namespace llarp
{
  struct udp_listener : public ev_io
  {
    llarp_udp_io* udp;

    udp_listener(int fd, llarp_udp_io* u) : ev_io(fd), udp(u){};

    ~udp_listener()
    {
    }

    bool
    tick();

    int
    read(byte_t* buf, size_t sz);

    int
    sendto(const sockaddr* to, const void* data, size_t sz);
  };

  struct tun : public ev_io
  {
    llarp_tun_io* t;
    int writefd;
    device* tunif;
    tun(llarp_tun_io* tio, llarp_ev_loop_ptr l)
        : ev_io(-1, new LossyWriteQueue_t("tun_write_queue", l, l))
        , t(tio)
        , writefd(-1)
        , tunif(tuntap_init())

              {

              };

    int
    sendto(const sockaddr* to, const void* data, size_t sz);

    bool
    tick();

    void
    flush_write();

    int
    read(byte_t* buf, size_t sz);

    bool
    setup();

    ~tun()
    {
      if(tunif)
        tuntap_destroy(tunif);
    }
  };
};  // namespace llarp

struct llarp_poll_loop
    : public llarp_ev_loop,
      public std::enable_shared_from_this< llarp_poll_loop >
{
  upoll_t* upollfd;

  llarp_poll_loop() : upollfd(nullptr)
  {
  }

  ~llarp_poll_loop()
  {
  }

  bool
  tcp_connect(struct llarp_tcp_connecter* tcp, const sockaddr* remoteaddr);

  llarp::ev_io*
  bind_tcp(llarp_tcp_acceptor* tcp, const sockaddr* bindaddr);

  virtual bool
  udp_listen(llarp_udp_io* l, const sockaddr* src);

  bool
  running() const;

  bool
  init();

  int
  tick(int ms);

  int
  run();

  int
  udp_bind(const sockaddr* addr);

  bool
  close_ev(llarp::ev_io* ev);

  llarp::ev_io*
  create_tun(llarp_tun_io* tun);

  llarp::ev_io*
  create_udp(llarp_udp_io* l, const sockaddr* src);

  bool
  add_ev(llarp::ev_io* e, bool write);

  bool
  udp_close(llarp_udp_io* l);

  void
  stop();
};

#endif
