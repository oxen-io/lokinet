#pragma once

#include "endpoint.hpp"

#include <optional>

namespace llarp::quic
{
  class Client : public Endpoint
  {
   public:
    // Constructs a client that establishes an outgoing connection to `remote` to tunnel packets to
    // `tunnel_port` on the remote's lokinet address. `local` can be used to optionally bind to a
    // local IP and/or port for the connection.
    Client(
        Address remote,
        std::shared_ptr<uvw::Loop> loop,
        uint16_t tunnel_port,
        std::optional<Address> local = std::nullopt);

    // Returns a reference to the client's connection to the server. Returns a nullptr if there is
    // no connection.
    std::shared_ptr<Connection>
    get_connection();

   private:
    void
    handle_packet(const Packet& p) override;
  };

}  // namespace llarp::quic
