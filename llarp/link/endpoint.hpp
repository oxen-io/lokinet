#pragma once

#include "connection.hpp"

#include <llarp/router/abstractrouter.hpp>
#include <llarp/router_id.hpp>

#include <external/oxen-libquic/include/quic.hpp>

namespace llarp::link
{
  struct Endpoint
  {
    Endpoint(std::shared_ptr<oxen::quic::Endpoint> ep, LinkManager& lm)
        : endpoint{std::move(ep)}, link_manager{lm}
    {}

    std::shared_ptr<oxen::quic::Endpoint> endpoint;
    LinkManager& link_manager;

    // for outgoing packets, we route via RouterID; map RouterID->Connection
    // for incoming packets, we get a ConnectionID; map ConnectionID->RouterID
    std::unordered_map<RouterID, std::shared_ptr<link::Connection>> conns;
    std::unordered_map<oxen::quic::ConnectionID, RouterID> connid_map;

    std::shared_ptr<link::Connection>
    get_conn(const RouterContact&) const;

    bool
    have_conn(const RouterID& remote, bool client_only) const;

    bool
    deregister_peer(RouterID remote);

    bool
    establish_connection(const oxen::quic::opt::local_addr& remote);
  };

}  // namespace llarp::link

/*
- Refactor RouterID to use gnutls info and maybe ConnectionID

*/
