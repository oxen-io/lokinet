#pragma once

#include "endpoint.hpp"
#include "llarp/endpoint_base.hpp"
#include <functional>

namespace llarp::quic
{
  class Server : public Endpoint
  {
   public:
    using stream_open_callback_t = std::function<bool(Stream& stream, uint16_t port)>;

    Server(EndpointBase& service_endpoint) : Endpoint{service_endpoint}
    {
      default_stream_buffer_size = 0;  // We don't currently use the endpoint ring buffer
    }

    // Stream callback: takes the server, the (just-created) stream, and the connection port.
    // Returns true if the stream should be allowed or false to reject the stream.  The callback
    // should set up the data_callback and close_callback on the stream: they will default to null
    // (which means incoming data will simply be dropped).
    stream_open_callback_t stream_open_callback;

   private:
    // Accept a new incoming connection, i.e. pre-handshake.  Returns a nullptr if the connection
    // can't be created (e.g. because of invalid initial data), or is invalid.
    std::shared_ptr<Connection>
    accept_initial_connection(const Packet& p) override;

    size_t
    write_packet_header(nuint16_t pport, uint8_t ecn) override;
  };

}  // namespace llarp::quic
