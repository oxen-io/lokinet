#ifndef LLARP_EV_VPNIO_HPP
#define LLARP_EV_VPNIO_HPP
#include <net/ip_packet.hpp>
#include <util/thread/queue.hpp>
#include <functional>

struct llarp_main;
struct llarp_vpn_io;

struct llarp_vpn_pkt_queue
{
  using Packet_t = llarp::net::IPPacket;
  llarp::thread::Queue<Packet_t> queue;

  llarp_vpn_pkt_queue() : queue(1024){};
  ~llarp_vpn_pkt_queue() = default;
};

struct llarp_vpn_pkt_writer : public llarp_vpn_pkt_queue
{
};

struct llarp_vpn_pkt_reader : public llarp_vpn_pkt_queue
{
};

struct llarp_vpn_io_impl
{
  llarp_vpn_io_impl(llarp_main* p, llarp_vpn_io* io) : ptr(p), parent(io)
  {
  }
  ~llarp_vpn_io_impl() = default;

  llarp_main* ptr;
  llarp_vpn_io* parent;

  llarp_vpn_pkt_writer writer;
  llarp_vpn_pkt_reader reader;

  void
  AsyncClose();

 private:
  void
  Expunge();
};

#endif
