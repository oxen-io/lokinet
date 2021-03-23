#pragma once

#include "endpoint.hpp"
#include "service/endpoint.hpp"

#include <optional>

namespace uvw
{
  struct ListenEvent;
  class TCPHandle;
}  // namespace uvw

namespace llarp::quic
{
  class Client : public Endpoint
  {
   public:
    // Constructs a client that establishes an outgoing connection to `remote` to tunnel packets to
    // `remote.getPort()` on the remote's lokinet address.  `pseudo_port` is *our* unique local
    // identifier which we include in outgoing packets (so that the remote server knows where to
    // send the back to *this* client).
    Client(service::Endpoint& ep, const SockAddr& remote, uint16_t pseudo_port);

    // Returns a reference to the client's connection to the server. Returns a nullptr if there is
    // no connection.
    std::shared_ptr<Connection>
    get_connection();

   private:
    size_t
    write_packet_header(nuint16_t remote_port, uint8_t ecn) override;
  };

}  // namespace llarp::quic
