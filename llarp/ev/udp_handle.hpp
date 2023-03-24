#include "ev.hpp"
#include "../util/buffer.hpp"
#include "llarp/net/sock_addr.hpp"

namespace llarp
{
  // Base type for UDP handling; constructed via EventLoop::make_udp().
  struct UDPHandle
  {
    using ReceiveFunc = EventLoop::UDPReceiveFunc;

    // Sends a packet to the given recipient, immediately.  Returns true if the send succeeded,
    // false it could not be performed (either because of error, or because it would have blocked).
    // If listen hasn't been called then a random IP/port will be used.
    virtual bool
    send(const SockAddr& dest, const llarp_buffer_t& buf) = 0;

    // Closes the listening UDP socket (if opened); this is typically called (automatically) during
    // destruction.  Does nothing if the UDP socket is already closed.
    virtual void
    close() = 0;

    // Returns the file descriptor of the socket, if available.  This generally exists only after
    // listen() has been called, and never exists on Windows.
    virtual std::optional<int>
    file_descriptor()
    {
      return std::nullopt;
    }

    /// returns the local address we are bound on
    virtual SockAddr
    LocalAddr() const = 0;

    // Base class destructor
    virtual ~UDPHandle() = default;

   protected:
    UDPHandle(ReceiveFunc on_recv) : on_recv{std::move(on_recv)}
    {
      // It makes no sense at all to use this with a null receive function:
      assert(this->on_recv);
    }

    // Callback to invoke when data is received
    ReceiveFunc on_recv;
  };
}  // namespace llarp
