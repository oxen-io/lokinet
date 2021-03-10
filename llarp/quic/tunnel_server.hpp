#pragma once

#include <net/sock_addr.hpp>
#include <service/address.hpp>
#include <ev/ev.hpp>

#include <memory>

namespace llarp::quic::tunnel
{
  enum class AcceptResult : int
  {
    ACCEPT = 0,    // Accepts a connection
    DECLINE = -1,  // Declines a connection (try other callbacks, refuse if all decline)
    REFUSE = -2,   // Refuses a connection (don't try any more callbacks)
  };

  // Class that wraps an incoming connection acceptance callback (to allow for callback removal).
  // This is not directly constructible: you must construct it via the TunnelServer instance.
  class IncomingTunnel final
  {
   public:
    using AcceptCallback = std::function<AcceptResult(
        const llarp::service::Address& remote, uint16_t port, llarp::SockAddr& connect_to)>;

   private:
    AcceptCallback accept;

    friend class TunnelServer;

    // Constructor with a full callback; invoked via TunnelServer::add_incoming_tunnel
    explicit IncomingTunnel(AcceptCallback accept) : accept{std::move(accept)}
    {}

    // Constructor for a simple forwarding to a single localhost port.  E.g. IncomingTunnel(22)
    // allows incoming connections to reach port 22 and forwards them to localhost:22.
    explicit IncomingTunnel(uint16_t localhost_port);

    // Constructor for forwarding everything to the same port; this is used by full clients by
    // default.
    IncomingTunnel();
  };

  // Class that handles incoming quic connections.  This class sets itself up in the llarp event
  // loop on construction and maintains a list of incoming acceptor callbacks.  When a new incoming
  // quic connections is being established we try the callbacks one by one to determine the local
  // TCP port the tunnel should be connected to until:
  // - a callback sets connect_to and returns AcceptResult::ACCEPT - we connect it to the returned
  //   address
  // - a callback returns AcceptResult::REFUSE - we reject the connection
  //
  // If a callback returns AcceptResult::DECLINE then we skip that callback and try the next one; if
  // all callbacks decline (or we have no callbacks at all) then we reject the connection.
  //
  // Note that tunnel operations and initialization are done in the event loop thread and so will
  // not take effect until the next event loop tick when called from some other thread.
  class TunnelServer : public std::enable_shared_from_this<TunnelServer>
  {
   public:
    explicit TunnelServer(EventLoop_ptr ev);

    // Appends a new tunnel to the end of the queue; all arguments are forwarded to private
    // constructor(s) of IncomingTunnel.
    template <typename... Args>
    std::shared_ptr<IncomingTunnel>
    add_incoming_tunnel(Args&&... args)
    {
      return std::shared_ptr<IncomingTunnel>{new IncomingTunnel{std::forward<Args>(args)...}};
    }

    // Removes a tunnel acceptor from the acceptor queue.
    void
    remove_incoming_tunnel(std::weak_ptr<IncomingTunnel> tunnel);

   private:
    EventLoop_ptr ev;
    std::vector<std::shared_ptr<IncomingTunnel>> tunnels;
  };

}  // namespace llarp::quic::tunnel
