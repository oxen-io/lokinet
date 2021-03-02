#ifndef LLARP_EV_HPP
#define LLARP_EV_HPP

#include <util/buffer.hpp>
#include <util/time.hpp>
#include <util/thread/threading.hpp>
#include <constants/evloop.hpp>

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
  class Logic;
  struct SockAddr;
  struct UDPHandle;

  namespace vpn
  {
    class NetworkInterface;
  }

  namespace net
  {
    struct IPPacket;
  }

  /// distinct event loop waker upper; used to idempotently schedule a task on the next event loop
  ///
  /// Created via EventLoop::make_waker(...).
  class EventLoopWakeup
  {
   public:
    /// Destructor: remove the task from the event loop task.  (Note that destruction here only
    /// initiates removal of the task from the underlying event loop: it is *possible* for the
    /// callback to fire again if already triggered depending on the underlying implementation).
    virtual ~EventLoopWakeup() = default;

    /// trigger this task to run on the next event loop iteration; does nothing if already
    /// triggered.
    virtual void
    Trigger() = 0;
  };

  /// holds a repeated task on the event loop; the task is removed on destruction
  class EventLoopRepeater
  {
   public:
    // Destructor: if the task has been started then it is removed from the event loop.  Note
    // that it is possible for a task to fire *after* destruction of this container;
    // destruction only initiates removal of the periodic task.
    virtual ~EventLoopRepeater() = default;

    // Starts the repeater to call `task` every `every` period.
    virtual void
    start(llarp_time_t every, std::function<void()> task) = 0;
  };

  // this (nearly!) abstract base class
  // is overriden for each platform
  struct EventLoop
  //      : std::enable_shared_from_this<EventLoop> // FIXME: do I actually need shared_from_this()?
  {
    // Runs the event loop. This does not return until sometime after `stop()` is called (and so
    // typically you want to run this in its own thread).
    void
    run(Logic& logic);

    // Actually runs the underlying implementation event loop; called by run().
    virtual void
    run_loop() = 0;

    virtual bool
    running() const = 0;

    virtual llarp_time_t
    time_now() const
    {
      return llarp::time_now_ms();
    }

    virtual void
    stopped()
    {}

    // Adds a timer to the event loop; should only be called from the logic thread (and so is
    // typically scheduled via a call to Logic::call_later()).
    virtual void
    call_after_delay(llarp_time_t delay_ms, std::function<void(void)> callback) = 0;

    virtual bool
    add_network_interface(
        std::shared_ptr<vpn::NetworkInterface> netif,
        std::function<void(net::IPPacket)> packetHandler) = 0;

    virtual bool
    add_ticker(std::function<void(void)> ticker) = 0;

    virtual void
    stop() = 0;

    using UDPReceiveFunc = std::function<void(UDPHandle&, SockAddr src, llarp::OwnedBuffer buf)>;

    // Constructs a UDP socket that can be used for sending and/or receiving
    virtual std::shared_ptr<UDPHandle>
    udp(UDPReceiveFunc on_recv) = 0;

    /// give this event loop a logic thread for calling
    virtual void
    set_logic(const std::shared_ptr<llarp::Logic>& logic) = 0;

    virtual ~EventLoop() = default;

    virtual void
    call_soon(std::function<void(void)> f) = 0;

    /// set the function that is called once per cycle the flush all the queues
    virtual void
    set_pump_function(std::function<void(void)> pumpll) = 0;

    /// Make a thread-safe event loop waker (an "async" in libuv terminology) on this event loop;
    /// you can call `->Trigger()` on the returned shared pointer to fire the callback at the next
    /// available event loop iteration.  (Multiple Trigger calls invoked before the call is actually
    /// made are coalesced into one call).
    virtual std::shared_ptr<EventLoopWakeup>
    make_waker(std::function<void()> callback) = 0;

    // Initializes a new repeated task object. Note that the task is not actually added to the event
    // loop until you call start() on the returned object.  Typically invoked via Logic::call_every.
    virtual std::shared_ptr<EventLoopRepeater>
    make_repeater() = 0;

    // Constructs and initializes a new default (libuv) event loop
    static std::shared_ptr<EventLoop>
    create(size_t queueLength = event_loop_queue_size);

    // Returns true if called from within the event loop thread, false otherwise.
    virtual bool
    inEventLoopThread() const = 0;
  };

  using EventLoop_ptr = std::shared_ptr<EventLoop>;

}  // namespace llarp
#endif
