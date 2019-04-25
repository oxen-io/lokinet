#ifndef EV_KQUEUE_HPP
#define EV_KQUEUE_HPP

#include <ev/ev.hpp>
#include <net/net.h>
#include <net/net.hpp>
#include <util/buffer.hpp>
#include <util/logger.hpp>

#include <sys/un.h>

// why did we need a macro here, kqueue(7) _only_ exists
// on BSD and Macintosh
#include <sys/event.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstdio>

namespace llarp
{
  struct udp_listener : public ev_io
  {
    llarp_udp_io* udp;

    udp_listener(int fd, llarp_udp_io* u) : ev_io(fd), udp(u)
    {
    }

    ~udp_listener()
    {
    }

    bool
    tick();

    virtual int
    read(byte_t* buf, size_t sz);

    virtual int
    sendto(const sockaddr* to, const void* data, size_t sz);
  };

  struct tun : public ev_io
  {
    llarp_tun_io* t;
    device* tunif;
    tun(llarp_tun_io* tio, llarp_ev_loop_ptr l)
        : ev_io(-1, new LossyWriteQueue_t("kqueue_tun_write", l, l))
        , t(tio)
        , tunif(tuntap_init())
    {
    }

    int
    sendto(__attribute__((unused)) const sockaddr* to,
           __attribute__((unused)) const void* data,
           __attribute__((unused)) size_t sz) override;

#ifdef __APPLE__
    ssize_t
    do_write(void* buf, size_t sz) override;
#endif

    void
    before_flush_write() override;

    bool
    tick() override;
    int
    read(byte_t* buf, size_t) override;

    bool
    setup();

    ~tun()
    {
      if(tunif)
        tuntap_destroy(tunif);
    }
  };

}  // namespace llarp

struct llarp_kqueue_loop final
    : public llarp_ev_loop,
      public std::enable_shared_from_this< llarp_kqueue_loop >
{
  int kqueuefd;

  llarp_kqueue_loop() : kqueuefd(-1)
  {
  }

  virtual ~llarp_kqueue_loop()
  {
  }

  bool
  init() override;

  int
  run() override;

  bool
  running() const override;

  bool
  tcp_connect(llarp_tcp_connecter* tcp, const sockaddr* addr) override;

  int
  tick(int ms) override;

  int
  udp_bind(const sockaddr* addr);

  virtual bool
  udp_listen(llarp_udp_io* l, const sockaddr* src) override;

  bool
  close_ev(llarp::ev_io* ev) override;

  llarp::ev_io*
  create_tun(llarp_tun_io* tun) override;

  llarp::ev_io*
  bind_tcp(llarp_tcp_acceptor* tcp, const sockaddr* bindaddr) override;

  llarp::ev_io*
  create_udp(llarp_udp_io* l, const sockaddr* src) override;

  bool
  add_ev(llarp::ev_io* ev, bool w) override;

  bool
  udp_close(llarp_udp_io* l) override;

  void
  stop() override;
};

#endif
