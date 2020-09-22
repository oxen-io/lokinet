#ifndef LLARP_H_
#define LLARP_H_
#include <stdint.h>
#include <unistd.h>

namespace llarp
{
  struct Context;
}

#ifdef __cplusplus
extern "C"
{
#endif

  /// packet writer to send packets to lokinet internals
  struct llarp_vpn_writer_pipe;
  /// packet reader to recv packets from lokinet internals
  struct llarp_vpn_reader_pipe;

  /// vpn io api
  /// all hooks called in native code
  /// for use with a vpn interface managed by external code
  /// the external vpn interface MUST be up and have addresses set
  struct llarp_vpn_io
  {
    /// private implementation
    void* impl;
    /// user data
    void* user;
    /// hook set by user called by lokinet core when lokinet is done with the
    /// vpn io
    void (*closed)(struct llarp_vpn_io*);
    /// hook set by user called from lokinet core after attempting to inject
    /// into endpoint passed a bool set to true if we were injected otherwise
    /// set to false
    void (*injected)(struct llarp_vpn_io*, bool);
    /// hook set by user called every event loop tick
    void (*tick)(struct llarp_vpn_io*);
  };

  /// info about the network interface that we give to lokinet core
  struct llarp_vpn_ifaddr_info
  {
    /// name of the network interface
    char ifname[64];
    /// interface's address as string
    char ifaddr[128];
    /// netmask number of bits set
    uint8_t netmask;
  };

  /// initialize llarp_vpn_io private implementation
  /// returns false if either parameter is nullptr
  bool
  llarp_vpn_io_init(llarp::Context* ctx, struct llarp_vpn_io* io);

  /// get the packet pipe for writing IP packets to lokinet internals
  /// returns nullptr if llarp_vpn_io is nullptr or not initialized
  struct llarp_vpn_pkt_writer*
  llarp_vpn_io_packet_writer(struct llarp_vpn_io* io);

  /// get the packet pipe for reading IP packets from lokinet internals
  /// returns nullptr if llarp_vpn_io is nullptr or not initialized
  struct llarp_vpn_pkt_reader*
  llarp_vpn_io_packet_reader(struct llarp_vpn_io* io);

  /// blocking read on packet reader from lokinet internals
  /// returns -1 on error, returns size of packet read
  /// thread safe
  ssize_t
  llarp_vpn_io_readpkt(struct llarp_vpn_pkt_reader* r, unsigned char* dst, size_t dstlen);

  /// blocking write on packet writer to lokinet internals
  /// returns false if we can't write this packet
  /// return true if we wrote this packet
  /// thread safe
  bool
  llarp_vpn_io_writepkt(struct llarp_vpn_pkt_writer* w, unsigned char* pktbuf, size_t pktlen);

  /// close vpn io and free private implementation after done
  /// operation is async and calls llarp_vpn_io.closed after fully closed
  /// after fully closed the llarp_vpn_io MUST be re-initialized by
  /// llarp_vpn_io_init if it is to be used again
  void
  llarp_vpn_io_close_async(struct llarp_vpn_io* io);

  /// give main context a vpn io for mobile when it is reader to do io with
  /// associated info tries to give the vpn io to endpoint with name epName a
  /// deferred call to llarp_vpn_io.injected is queued unconditionally
  /// thread safe
  bool
  llarp_main_inject_vpn_by_name(
      struct llarp::Context* m,
      const char* epName,
      struct llarp_vpn_io* io,
      struct llarp_vpn_ifaddr_info info);

  /// give main context a vpn io on its default endpoint
  static bool
  llarp_main_inject_default_vpn(
      struct llarp::Context* m, struct llarp_vpn_io* io, struct llarp_vpn_ifaddr_info info)
  {
    return llarp_main_inject_vpn_by_name(m, "default", io, info);
  }

#ifdef __cplusplus
}
#endif
#endif
