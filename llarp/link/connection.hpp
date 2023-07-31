#pragma once

#include <llarp/router_id.hpp>
#include <llarp/router_contact.hpp>

#include <external/oxen-libquic/include/quic.hpp>

namespace llarp::link
{

  class Connection
  {
    std::shared_ptr<oxen::quic::connection_interface> conn;

    RouterID remote_id;
    RouterContact remote_rc;
    AddressInfo remote_addr_info; // RC may have many, this is the one in use for this connection

    bool inbound; // one side of a connection will be responsible for some things, e.g. heartbeat
    bool remote_is_relay;

    public:

      const RouterContact& RemoteRC() { return remote_rc; }
      const RouterID& RemoteID() { return remote_id; }

      bool RemoteIsRelay() { return remote_is_relay; }

  };
}  // namespace llarp::link
