#ifndef LLARP_EV_HPP
#define LLARP_EV_HPP

#include <net/ip_address.hpp>
#include <net/ip_packet.hpp>
#include <ev/ev.h>
#include <util/buffer.hpp>
#include <util/codel.hpp>
#include <util/thread/threading.hpp>

// writev
#ifndef _WIN32
#include <sys/uio.h>
#include <unistd.h>
#endif

#include <algorithm>
#include <deque>
#include <list>
#include <future>
#include <utility>

#ifdef _WIN32
// From the preview SDK, should take a look at that
// periodically in case its definition changes
#define UNIX_PATH_MAX 108

typedef struct sockaddr_un
{
  ADDRESS_FAMILY sun_family;    /* AF_UNIX */
  char sun_path[UNIX_PATH_MAX]; /* pathname */
} SOCKADDR_UN, *PSOCKADDR_UN;
#else

#if defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__) || (__APPLE__ && __MACH__)
#include <sys/event.h>
#endif

#include <sys/un.h>
#endif

struct llarp_ev_pkt_pipe;

#ifndef MAX_WRITE_QUEUE_SIZE
#define MAX_WRITE_QUEUE_SIZE (1024UL)
#endif

#ifndef EV_READ_BUF_SZ
#define EV_READ_BUF_SZ (4 * 1024UL)
#endif
#ifndef EV_WRITE_BUF_SZ
#define EV_WRITE_BUF_SZ (4 * 1024UL)
#endif

namespace llarp
{
  namespace vpn
  {
    class NetworkInterface;
  }

  // this (nearly!) abstract base class
  // is overriden for each platform
  struct EventLoop
  {
    byte_t readbuf[EV_READ_BUF_SZ] = {0};

    virtual bool
    init() = 0;

    virtual int
    run() = 0;

    virtual bool
    running() const = 0;

    virtual void
    update_time()
    {}

    virtual llarp_time_t
    time_now() const
    {
      return llarp::time_now_ms();
    }

    virtual void
    stopped(){};

    virtual int
    tick(int ms) = 0;

    virtual uint32_t
    call_after_delay(llarp_time_t delay_ms, std::function<void(void)> callback) = 0;

    virtual void
    cancel_delayed_call(uint32_t call_id) = 0;

    virtual bool
    add_network_interface(
        std::shared_ptr<vpn::NetworkInterface> netif,
        std::function<void(net::IPPacket)> packetHandler) = 0;

    virtual bool
    add_ticker(std::function<void(void)> ticker) = 0;

    virtual void
    stop() = 0;

    virtual bool
    udp_listen(llarp_udp_io* l, const llarp::SockAddr& src) = 0;

    virtual bool
    udp_close(llarp_udp_io* l) = 0;

    /// give this event loop a logic thread for calling
    virtual void set_logic(std::shared_ptr<llarp::Logic>) = 0;

    virtual ~EventLoop() = default;

    virtual void
    call_soon(std::function<void(void)> f) = 0;

    virtual void
    register_poll_fd_readable(int fd, std::function<void(void)> callback) = 0;

    virtual void
    deregister_poll_fd_readable(int fd) = 0;
  };
}  // namespace llarp
#endif
