#ifndef LLARP_EV_H
#define LLARP_EV_H
#if defined(__MINGW32__) || defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#include <wspiapi.h>
#ifndef ssize_t
#define ssize_t long
#endif
#else

#include <netinet/in.h>
#include <sys/socket.h>
#endif
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <tuntap.h>
/**
 * ev.h
 *
 * event handler (cross platform high performance event system for IO)
 */

// forward declare
struct llarp_threadpool;
struct llarp_logic;

struct llarp_ev_loop;

/// allocator
void
llarp_ev_loop_alloc(struct llarp_ev_loop **ev);

// deallocator
void
llarp_ev_loop_free(struct llarp_ev_loop **ev);

/// run main loop
int
llarp_ev_loop_run(struct llarp_ev_loop *ev, struct llarp_logic *logic);

void
llarp_ev_loop_run_single_process(struct llarp_ev_loop *ev,
                                 struct llarp_threadpool *tp,
                                 struct llarp_logic *logic);

/// stop event loop and wait for it to complete all jobs
void
llarp_ev_loop_stop(struct llarp_ev_loop *ev);

/// UDP handling configuration
struct llarp_udp_io
{
  /// set after added
  int fd;
  void *user;
  void *impl;
  struct llarp_ev_loop *parent;
  /// called every event loop tick after reads
  void (*tick)(struct llarp_udp_io *);
  // sockaddr * is the source
  void (*recvfrom)(struct llarp_udp_io *, const struct sockaddr *, const void *,
                   ssize_t);
};

/// add UDP handler
int
llarp_ev_add_udp(struct llarp_ev_loop *ev, struct llarp_udp_io *udp,
                 const struct sockaddr *src);

/// schedule UDP packet
int
llarp_ev_udp_sendto(struct llarp_udp_io *udp, const struct sockaddr *to,
                    const void *data, size_t sz);

/// close UDP handler
int
llarp_ev_close_udp(struct llarp_udp_io *udp);

// forward declare
struct llarp_tcp_acceptor;

/// a single tcp connection
struct llarp_tcp_conn
{
  /// user data
  void *user;
  /// private implementation
  void *impl;
  /// parent loop (dont set me)
  struct llarp_ev_loop *loop;
  /// handle read event
  void (*read)(struct llarp_tcp_conn *, const void *, size_t);
  /// handle close event (free-ing is handled by event loop)
  void (*closed)(struct llarp_tcp_conn *);
};

/// queue async write a buffer in full
void
llarp_tcp_conn_async_write(struct llarp_tcp_conn *, const void *, size_t);

/// close a tcp connection
void
llarp_tcp_conn_close(struct llarp_tcp_conn *);

struct llarp_tcp_acceptor
{
  /// userdata pointer
  void *user;
  /// internal implementation
  void *impl;
  /// parent event loop (dont set me)
  struct llarp_ev_loop *loop;
  /// handle inbound connection
  void (*accepted)(struct llarp_tcp_acceptor *, struct llarp_tcp_conn *);
};

/// bind to an address and start serving async
/// return false if failed to bind
/// return true on successs
bool
llarp_tcp_serve(struct llarp_tcp_acceptor *t, const sockaddr *bindaddr);

/// close and stop accepting connections
void
llarp_tcp_acceptor_close(struct llarp_tcp_acceptor *);

#ifdef _WIN32
#define IFNAMSIZ (16)
#endif

struct llarp_tun_io
{
  // TODO: more info?
  char ifaddr[128];
  int netmask;
  char ifname[IFNAMSIZ + 1];

  void *user;
  void *impl;
  struct llarp_ev_loop *parent;
  /// called when we are able to write right before we write
  /// this happens after reading packets
  void (*before_write)(struct llarp_tun_io *);
  /// called every event loop tick after reads
  void (*tick)(struct llarp_tun_io *);
  void (*recvpkt)(struct llarp_tun_io *, const void *, ssize_t);
};

/// create tun interface with network interface name ifname
/// returns true on success otherwise returns false
bool
llarp_ev_add_tun(struct llarp_ev_loop *ev, struct llarp_tun_io *tun);

/// async write a packet on tun interface
/// returns true if queued, returns false on drop
bool
llarp_ev_tun_async_write(struct llarp_tun_io *tun, const void *pkt, size_t sz);

#endif
