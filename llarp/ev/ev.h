#ifndef LLARP_EV_H
#define LLARP_EV_H

#include <net/ip_address.hpp>
#include <util/buffer.hpp>
#include <util/time.hpp>

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

#include <constants/evloop.hpp>

/**
 * ev.h
 *
 * event handler (cross platform high performance event system for IO)
 */

#define EV_TICK_INTERVAL 10

namespace llarp
{
  class Logic;
  struct EventLoop;
}  // namespace llarp

using llarp_ev_loop_ptr = std::shared_ptr<llarp::EventLoop>;

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
  llarp::EventLoop* parent;

  /// called every event loop tick after reads
  void (*tick)(struct llarp_udp_io*);

  void (*recvfrom)(struct llarp_udp_io*, const llarp::SockAddr& source, ManagedBuffer);
  /// set by parent
  int (*sendto)(struct llarp_udp_io*, const llarp::SockAddr&, const byte_t*, size_t);
};

/// add UDP handler
int
llarp_ev_add_udp(const llarp_ev_loop_ptr& ev, struct llarp_udp_io* udp, const llarp::SockAddr& src);

/// send a UDP packet
int
llarp_ev_udp_sendto(struct llarp_udp_io* udp, const llarp::SockAddr& to, const llarp_buffer_t& pkt);

/// close UDP handler
int
llarp_ev_close_udp(struct llarp_udp_io* udp);

#endif
