#pragma once

#include "endpoint.hpp"

#include <functional>

namespace llarp::quic
{
  class Server : public Endpoint
  {
   public:
    using stream_open_callback_t =
        std::function<bool(Server& server, Stream& stream, uint16_t port)>;

    Server(Address listen, std::shared_ptr<uvw::Loop> loop, stream_open_callback_t stream_opened);

    // Stream callback: takes the server, the (just-created) stream, and the connection port.
    // Returns true if the stream should be allowed or false to reject the stream.  The callback
    // should set up the data_callback and close_callback on the stream: they will default to null
    // (which means incoming data will simply be dropped).
    stream_open_callback_t stream_open_callback;

    int
    setup_null_crypto(ngtcp2_conn* conn);

   private:
    // Handles an incoming packet by figuring out and handling the connection id; if necessary we
    // send back a version negotiation or a connection close frame, or drop the packet (if in the
    // draining state).  If we get through all of the above then it's a packet to read, in which
    // case we pass it on to read_packet().
    void
    handle_packet(const Packet& p) override;

    // Creates a new connection from an incoming packet.  Returns a nullptr if the connection can't
    // be created.
    std::shared_ptr<Connection>
    accept_connection(const Packet& p);
  };

}  // namespace llarp::quic
