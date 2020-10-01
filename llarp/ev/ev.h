#ifndef LLARP_EV_H
#define LLARP_EV_H

#include <net/ip_address.hpp>
#include <util/buffer.hpp>
#include <util/time.hpp>
#include <tuntap.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <wspiapi.h>
#else
#include <netinet/in.h>
#include <sys/socket.h>
#endif
#include <net/net_if.hpp>

#include <memory>

#include <cstdint>
#include <cstdlib>

#if !defined(WIN32)
#include <uv.h>
#endif

#include <constants/evloop.hpp>

/**
 * ev.h
 *
 * event handler (cross platform high performance event system for IO)
 */

#define EV_TICK_INTERVAL 10

// forward declare
struct llarp_threadpool;

struct llarp_ev_loop;

namespace llarp
{
  class Logic;
}

using llarp_ev_loop_ptr = std::shared_ptr<llarp_ev_loop>;

/// make an event loop using our baked in event loop on Windows
/// make an event loop using libuv otherwise.
/// @param queue_size how big the logic job queue is
llarp_ev_loop_ptr
llarp_make_ev_loop(std::size_t queue_size = llarp::event_loop_queue_size);

// run mainloop
void
llarp_ev_loop_run_single_process(llarp_ev_loop_ptr ev, std::shared_ptr<llarp::Logic> logic);

/// get the current time on the event loop
llarp_time_t
llarp_ev_loop_time_now_ms(const llarp_ev_loop_ptr& ev);

/// stop event loop and wait for it to complete all jobs
void
llarp_ev_loop_stop(const llarp_ev_loop_ptr& ev);

/// UDP handling configuration
struct llarp_udp_io
{
  /// set after added
  int fd;
  void* user;
  void* impl;
  struct llarp_ev_loop* parent;

  /// called every event loop tick after reads
  void (*tick)(struct llarp_udp_io*);

  void (*recvfrom)(struct llarp_udp_io*, const llarp::SockAddr& source, ManagedBuffer);
  /// set by parent
  int (*sendto)(struct llarp_udp_io*, const llarp::SockAddr&, const byte_t*, size_t);
};

/// add UDP handler
int
llarp_ev_add_udp(struct llarp_ev_loop* ev, struct llarp_udp_io* udp, const llarp::SockAddr& src);

/// send a UDP packet
int
llarp_ev_udp_sendto(struct llarp_udp_io* udp, const llarp::SockAddr& to, const llarp_buffer_t& pkt);

/// close UDP handler
int
llarp_ev_close_udp(struct llarp_udp_io* udp);

// forward declare
struct llarp_tcp_acceptor;

/// a single tcp connection
struct llarp_tcp_conn
{
  /// user data
  void* user;
  /// private implementation
  void* impl;
  /// parent loop (dont set me)
  struct llarp_ev_loop* loop;
  /// handle read event
  void (*read)(struct llarp_tcp_conn*, const llarp_buffer_t&);
  //// set by parent
  ssize_t (*write)(struct llarp_tcp_conn*, const byte_t*, size_t sz);
  /// set by parent
  bool (*is_open)(struct llarp_tcp_conn*);
  /// handle close event (free-ing is handled by event loop)
  void (*closed)(struct llarp_tcp_conn*);
  /// explict close by user (set by parent)
  void (*close)(struct llarp_tcp_conn*);
  /// handle event loop tick
  void (*tick)(struct llarp_tcp_conn*);
};

/// queue async write a buffer in full
/// return if we queued it or not
bool
llarp_tcp_conn_async_write(struct llarp_tcp_conn*, const llarp_buffer_t&);

/// close a tcp connection
void
llarp_tcp_conn_close(struct llarp_tcp_conn*);

/// handles outbound connections to 1 endpoint
struct llarp_tcp_connecter
{
  /// remote address family
  int af;
  /// remote address string
  llarp::IpAddress remote;
  /// userdata pointer
  void* user;
  /// private implementation (dont set me)
  void* impl;
  /// parent event loop (dont set me)
  struct llarp_ev_loop* loop;
  /// handle outbound connection made
  void (*connected)(struct llarp_tcp_connecter*, struct llarp_tcp_conn*);
  /// handle outbound connection error
  void (*error)(struct llarp_tcp_connecter*);
};

/// async try connecting to a remote connection 1 time
void
llarp_tcp_async_try_connect(struct llarp_ev_loop* l, struct llarp_tcp_connecter* tcp);

/// handles inbound connections
struct llarp_tcp_acceptor
{
  /// userdata pointer
  void* user;
  /// internal implementation
  void* impl;
  /// parent event loop (dont set me)
  struct llarp_ev_loop* loop;
  /// handle event loop tick
  void (*tick)(struct llarp_tcp_acceptor*);
  /// handle inbound connection
  void (*accepted)(struct llarp_tcp_acceptor*, struct llarp_tcp_conn*);
  /// handle after server socket closed (free-ing is handled by event loop)
  void (*closed)(struct llarp_tcp_acceptor*);
  /// set by impl
  void (*close)(struct llarp_tcp_acceptor*);
};

/// bind to an address and start serving async
/// return false if failed to bind
/// return true on success
bool
llarp_tcp_serve(
    struct llarp_ev_loop* loop, struct llarp_tcp_acceptor* t, const llarp::SockAddr& bindaddr);

/// close and stop accepting connections
void
llarp_tcp_acceptor_close(struct llarp_tcp_acceptor*);

#ifdef _WIN32
#define IFNAMSIZ (16)
#endif

struct llarp_fd_promise;

/// wait until the fd promise is set
int
llarp_fd_promise_wait_for_value(struct llarp_fd_promise* promise);

struct llarp_tun_io
{
  // TODO: more info?
  char ifaddr[128];
  // windows only
  uint32_t dnsaddr;
  int netmask;
  char ifname[IFNAMSIZ + 1];

  void* user;
  void* impl;

  /// functor for getting a promise that returns the vpn fd
  /// dont set me if you don't know how to use this
  struct llarp_fd_promise* (*get_fd_promise)(struct llarp_tun_io*);

  struct llarp_ev_loop* parent;
  /// called when we are able to write right before we write
  /// this happens after reading packets
  void (*before_write)(struct llarp_tun_io*);
  /// called every event loop tick after reads
  void (*tick)(struct llarp_tun_io*);
  void (*recvpkt)(struct llarp_tun_io*, const llarp_buffer_t&);
  /// set by parent
  bool (*writepkt)(struct llarp_tun_io*, const byte_t*, size_t);
};

/// create tun interface with network interface name ifname
/// returns true on success otherwise returns false
bool
llarp_ev_add_tun(struct llarp_ev_loop* ev, struct llarp_tun_io* tun);

/// async write a packet on tun interface
/// returns true if queued, returns false on drop
bool
llarp_ev_tun_async_write(struct llarp_tun_io* tun, const llarp_buffer_t&);

#endif
