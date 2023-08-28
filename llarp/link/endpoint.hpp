#pragma once

#include "connection.hpp"

#include <llarp/router/abstractrouter.hpp>
#include <llarp/router_id.hpp>

#include <external/oxen-libquic/include/quic.hpp>

namespace llarp::link
{
  struct Endpoint
  {
    std::shared_ptr<oxen::quic::Endpoint> endpoint;
    bool inbound {false};

    // for outgoing packets, we route via RouterID; map RouterID->Connection
    // for incoming packets, we get a ConnectionID; map ConnectionID->RouterID
    std::unordered_map<RouterID, link::Connection> connections;
    std::unordered_map<oxen::quic::ConnectionID, RouterID> connid_map;

  };

}  // namespace llarp::link
