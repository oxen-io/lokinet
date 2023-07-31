#pragma once

#include <llarp/router_id.hpp>

#include <oxen-libquic/include/quic.hpp>

namespace llarp::link
{

#ifndef NDEBUG
  struct debug_hooks
  {
    oxen::quic::dgram_data_callback incoming_datagram;
    oxen::quic::stream_data_callback incoming_stream_packet;
  };
#endif

  class Endpoint
  {
    std::shared_ptr<oxen::quic::Endpoint> endpoint;

    // for outgoing packets, we route via RouterID; map RouterID->Connection
    // for incoming packets, we get a ConnectionID; map ConnectionID->RouterID
    std::unordered_map<RouterID, std::shared_ptr<llarp::link::Connection>> connections;
    std::unordered_map<oxen::quic::ConnectionID, RouterID> connid_map;

    AbstractRouter* router;
    oxen::quic::Address bind_addr;

    public:

#ifndef NDEBUG
    debug_hooks debug;
    // could obviously set directly because public, but setter doesn't hurt
    void SetDebugHooks(debug_hooks hooks) { debug = std::move(hooks); }
#endif

    Endpoint(AbstractRouter* router, oxen::quic::Address bind_addr);

    // Establish a connection to the remote `rc`.
    //
    // If already connected (or pending), returns existing connection.
    // If connection not possible (e.g. no suitable remote address), returns nullptr.
    // Otherwise, creates and returns Connection, usable right away.
    std::shared_ptr<llarp::link::Connection> Connect(RouterContact rc);

    // Return the Connection to `remote` if we have one, else nullptr.
    std::shared_ptr<llarp::link::Connection> GetConnection(RouterID remote);

    void HandleIncomingDataMessage(oxen::quic::dgram_interface& dgi, bstring dgram);
    void HandleIncomingControlMessage(oxen::quic::Stream& stream, bstring_view packet);
  };

}  // namespace llarp::link
