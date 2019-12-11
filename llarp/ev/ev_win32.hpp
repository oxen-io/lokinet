#ifndef EV_WIN32_H
#define EV_WIN32_H
#ifdef _WIN32
#include <ev/ev.hpp>
#include <net/net.h>
#include <net/net.hpp>
#include <util/buffer.hpp>
#include <util/thread/logic.hpp>

#include <windows.h>
#include <process.h>

#include <cstdio>

// io packet for TUN read/write
struct asio_evt_pkt
{
  OVERLAPPED pkt = {
      0, 0, 0, 0, nullptr};  // must be first, since this is part of the IO call
  bool write = false;        // true, or false if read pkt
  size_t sz;  // should match the queued data size, if not try again?
  void* buf;  // must remain valid until we get notification; this is _supposed_
              // to be zero-copy
};

extern "C" DWORD FAR PASCAL
tun_ev_loop(void* unused);

void
exit_tun_loop();

void
begin_tun_loop(int nThreads);

namespace llarp
{
  struct udp_listener : public ev_io
  {
    llarp_udp_io* udp;
    llarp_pkt_list m_RecvPackets;

    udp_listener(int fd, llarp_udp_io* u) : ev_io(fd), udp(u){};

    ~udp_listener()
    {
    }

    bool
    RecvMany(llarp_pkt_list*);

    bool
    tick();

    int
    read(byte_t* buf, size_t sz);

    int
    sendto(const sockaddr* to, const void* data, size_t sz);
  };
}  // namespace llarp

// A different kind of event loop,
// more suited for the native Windows NT
// event model
struct win32_tun_io
{
  llarp_tun_io* t;
  device* tunif;
  byte_t readbuf[EV_READ_BUF_SZ] = {0};

  win32_tun_io(llarp_tun_io* tio) : t(tio), tunif(tuntap_init()){};

  bool
  queue_write(const byte_t* buf, size_t sz);

  bool
  setup();

  // first TUN device gets to set up the event port
  bool
  add_ev(llarp_ev_loop* l);

  // places data in event queue for kernel to process
  void
  do_write(void* data, size_t sz);

  // we call this one when we get a packet in the event port
  // which then kicks off another write
  void
  flush_write();

  void
  read(byte_t* buf, size_t sz);

  ~win32_tun_io()
  {
    CancelIo(tunif->tun_fd);
    if(tunif->tun_fd)
      tuntap_destroy(tunif);
  }
};

// UDP event loop (no longer used, we libuv for now)
// PLEASE convert me to IOCPs so we don't have to use that
// EXTREMELY CURSED libuv event loop and its llarp_vpn_io_pipe
// 
// For you see, on Windows, we have enough local user permissions to set
// up a VPN tunnel interface internally, and have lokinet consume this
// file descriptor directly. See win32_tun_io for this impl. llarp_vpn_io
// assumes that an external entity or process is required to inject packets
// into the VPN interface provided by the OS.
//
// Not only that, the win32 IOCP facility handles timing on its own, you can
// specify an interval to tick directly into the call to GetQueuedCompletionStatus(2)
struct llarp_win32_loop : public llarp_ev_loop
{
  upoll_t* upollfd;
  std::shared_ptr< llarp::Logic > m_Logic;
  std::vector< std::function< void(void) > > m_Tickers;

  llarp_win32_loop() : upollfd(nullptr)
  {
  }

  bool
  tcp_connect(struct llarp_tcp_connecter* tcp, const sockaddr* remoteaddr);

  llarp::ev_io*
  bind_tcp(llarp_tcp_acceptor* tcp, const sockaddr* bindaddr);

  virtual bool
  udp_listen(llarp_udp_io* l, const sockaddr* src);

  ~llarp_win32_loop();

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

  // no tunnels here
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

  bool
  add_ticker(std::function< void(void) > func) override
  {
    m_Tickers.emplace_back(func);
    return true;
  }

  void
  set_logic(std::shared_ptr< llarp::Logic > l) override
  {
    m_Logic = l;
  }

  void
  tick_listeners() override;
};

#endif
#endif
