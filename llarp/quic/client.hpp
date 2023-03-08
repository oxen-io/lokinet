#pragma once

#include "endpoint.hpp"
#include "llarp/endpoint_base.hpp"

#include <optional>

namespace llarp::quic
{
  class Client : public Endpoint
  {
   public:
    // Constructs a client that establishes an outgoing connection to `remote` to tunnel packets to
    // `remote.getPort()` on the remote's lokinet address.  `pseudo_port` is *our* unique local
    // identifier which we include in outgoing packets (so that the remote server knows where to
    // send the back to *this* client).
    Client(
        EndpointBase& ep,
        const uint16_t port,
        std::variant<service::Address, RouterID>&& remote,
        uint16_t pseudo_port);

    // Returns a reference to the client's connection to the server. Returns a nullptr if there is
    // no connection.
    std::shared_ptr<Connection>
    get_connection();

   private:
    size_t
    write_packet_header(nuint16_t remote_port, uint8_t ecn) override;
  };

}  // namespace llarp::quic
