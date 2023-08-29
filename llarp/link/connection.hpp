#pragma once

#include <llarp/router_id.hpp>
#include <llarp/router_contact.hpp>

#include <external/oxen-libquic/include/quic.hpp>

namespace llarp::link
{

  struct Connection
  {
    std::shared_ptr<oxen::quic::connection_interface> conn;
    std::shared_ptr<oxen::quic::Stream> control_stream;

    RouterContact remote_rc;

    bool inbound;  // one side of a connection will be responsible for some things, e.g. heartbeat
    bool remote_is_relay;
  };
}  // namespace llarp::link
