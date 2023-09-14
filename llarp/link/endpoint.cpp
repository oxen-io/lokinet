#include "endpoint.hpp"
#include "link_manager.hpp"

namespace llarp::link
{
  std::shared_ptr<link::Connection>
  Endpoint::get_conn(const RouterContact& rc) const
  {
    for (const auto& [rid, conn] : conns)
    {
      if (conn->remote_rc == rc)
        return conn;
    }

    return nullptr;
  }

  bool
  Endpoint::have_conn(const RouterID& remote, bool client_only) const
  {
    if (auto itr = conns.find(remote); itr != conns.end())
    {
      if (not(itr->second->remote_is_relay and client_only))
        return true;
    }

    return false;
  }

  // TOFIX: use the new close methods after bumping libquic
  bool
  Endpoint::deregister_peer(RouterID remote)
  {
    if (auto itr = conns.find(remote); itr != conns.end())
    {
      endpoint->close_connection(*dynamic_cast<oxen::quic::Connection*>(itr->second->conn.get()));
      conns.erase(itr);
      return true;
    }

    return false;
  }

  bool
  Endpoint::establish_connection(const oxen::quic::opt::local_addr& remote)
  {
    try
    {
      oxen::quic::dgram_data_callback dgram_cb =
          [this](oxen::quic::dgram_interface& dgi, bstring dgram) {
            link_manager.recv_data_message(dgi, dgram);
          };

      auto conn_interface = endpoint->connect(remote, link_manager.tls_creds, dgram_cb);
      auto control_stream = conn_interface->get_new_stream();

      // TOFIX: get a real RouterID after refactoring it
      RouterID rid;
      auto [itr, b] = conns.emplace(rid);
      itr->second = std::make_shared<link::Connection>(conn_interface, control_stream);
      connid_map.emplace(conn_interface->scid(), rid);

      return true;
    }
    catch (...)
    {
      log::error(quic_cat, "Error: failed to establish connection to {}", remote);
      return false;
    }
  }

}  // namespace llarp::link
