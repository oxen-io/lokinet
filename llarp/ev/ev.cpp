#include <ev/ev.h>
#include <util/mem.hpp>
#include <util/str.hpp>
#include <util/thread/logic.hpp>

#include <cstddef>
#include <cstring>
#include <string_view>

// We libuv now
#include <ev/ev_libuv.hpp>

llarp_ev_loop_ptr
llarp_make_ev_loop(size_t queueLength)
{
  llarp_ev_loop_ptr r = std::make_shared<libuv::Loop>(queueLength);
  r->init();
  r->update_time();
  return r;
}

void
llarp_ev_loop_run_single_process(llarp_ev_loop_ptr ev, std::shared_ptr<llarp::Logic> logic)
{
  if (ev == nullptr or logic == nullptr)
    return;
  ev->run();
  logic->clear_event_loop();
  ev->stopped();
}

int
llarp_ev_add_udp(const llarp_ev_loop_ptr& ev, struct llarp_udp_io* udp, const llarp::SockAddr& src)
{
  if (ev == nullptr or udp == nullptr)
  {
    llarp::LogError("Attempting llarp_ev_add_udp() with null event loop or udp io struct.");
    return -1;
  }
  udp->parent = ev.get();
  if (ev->udp_listen(udp, src))
    return 0;
  llarp::LogError("llarp_ev_add_udp() call to udp_listen failed.");
  return -1;
}

int
llarp_ev_close_udp(struct llarp_udp_io* udp)
{
  if (udp->parent->udp_close(udp))
    return 0;
  return -1;
}

llarp_time_t
llarp_ev_loop_time_now_ms(const llarp_ev_loop_ptr& loop)
{
  if (loop)
    return loop->time_now();
  return llarp::time_now_ms();
}

void
llarp_ev_loop_stop(const llarp_ev_loop_ptr& loop)
{
  loop->stop();
}

int
llarp_ev_udp_sendto(struct llarp_udp_io* udp, const llarp::SockAddr& to, const llarp_buffer_t& buf)
{
  return udp->sendto(udp, to, buf.base, buf.sz);
}
