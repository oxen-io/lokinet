#include "link_manager.hpp"

#include <llarp/router/router.hpp>
#include <llarp/router/rc_lookup_handler.hpp>
#include <llarp/nodedb.hpp>
#include <llarp/crypto/crypto.hpp>

#include <algorithm>
#include <set>

namespace llarp
{

  LinkManager::LinkManager(Router& r)
      : router{r}
      , quic{std::make_unique<oxen::quic::Network>()}
      , tls_creds{oxen::quic::GNUTLSCreds::make_from_ed_keys(
            {reinterpret_cast<const char*>(router.encryption().data()), router.encryption().size()},
            {reinterpret_cast<const char*>(router.encryption().toPublic().data()), size_t{32}})}
      , ep{quic->endpoint(router.local), *this}
  {}

  std::shared_ptr<link::Connection>
  LinkManager::get_compatible_link(const RouterContact& rc)
  {
    if (stopping)
      return nullptr;

    if (auto c = ep.get_conn(rc); c)
      return c;

    return nullptr;
  }

  // TODO: replace with control/data message sending with libquic
  bool
  LinkManager::send_to(
      const RouterID& remote,
      const llarp_buffer_t&,
      AbstractLinkSession::CompletionHandler completed,
      uint16_t)
  {
    if (stopping)
      return false;

    if (not have_connection_to(remote))
    {
      if (completed)
      {
        completed(AbstractLinkSession::DeliveryStatus::eDeliveryDropped);
      }
      return false;
    }

    // TODO: send the message
    // TODO: if we keep bool return type, change this accordingly
    return false;
  }

  bool
  LinkManager::have_connection_to(const RouterID& remote, bool client_only) const
  {
    return ep.have_conn(remote, client_only);
  }

  bool
  LinkManager::have_client_connection_to(const RouterID& remote) const
  {
    return ep.have_conn(remote, true);
  }

  void
  LinkManager::deregister_peer(RouterID remote)
  {
    if (auto rv = ep.deregister_peer(remote); rv)
    {
      persisting_conns.erase(remote);
      log::info(logcat, "Peer {} successfully de-registered");
    }
    else
      log::warning(logcat, "Peer {} not found for de-registeration!");
  }

  void
  LinkManager::connect_to(const oxen::quic::opt::local_addr& remote)
  {
    if (auto rv = ep.establish_connection(remote); rv)
      log::info(quic_cat, "Connection to {} successfully established!", remote);
    else
      log::info(quic_cat, "Connection to {} unsuccessfully established", remote);
  }

  void
  LinkManager::stop()
  {
    if (stopping)
    {
      return;
    }

    util::Lock l(_mutex);

    LogInfo("stopping links");
    stopping = true;

    quic.reset();
  }

  void
  LinkManager::set_conn_persist(const RouterID& remote, llarp_time_t until)
  {
    if (stopping)
      return;

    util::Lock l(_mutex);

    persisting_conns[remote] = std::max(until, persisting_conns[remote]);
    if (have_client_connection_to(remote))
    {
      // mark this as a client so we don't try to back connect
      clients.Upsert(remote);
    }
  }

  size_t
  LinkManager::get_num_connected(bool clients_only) const
  {
    size_t count{0};
    for (const auto& ep : endpoints)
    {
      for (const auto& conn : ep.connections)
      {
        if (not(conn.second.remote_is_relay and clients_only))
          count++;
      }
    }

    return count;
  }

  size_t
  LinkManager::get_num_connected_clients() const
  {
    return get_num_connected(true);
  }

  bool
  LinkManager::get_random_connected(RouterContact& router) const
  {
    std::unordered_map<RouterID, RouterContact> connectedRouters;

    for (const auto& ep : endpoints)
    {
      for (const auto& [router_id, conn] : ep.connections)
      {
        connectedRouters.emplace(router_id, conn.remote_rc);
      }
    }

    const auto sz = connectedRouters.size();
    if (sz)
    {
      auto itr = connectedRouters.begin();
      if (sz > 1)
      {
        std::advance(itr, randint() % sz);
      }

      router = itr->second;

      return true;
    }

    return false;
  }

  // TODO: this?  perhaps no longer necessary in the same way?
  void
  LinkManager::check_persisting_conns(llarp_time_t)
  {
    if (stopping)
      return;
  }

  // TODO: do we still need this concept?
  void
  LinkManager::update_peer_db(std::shared_ptr<PeerDb>)
  {}

  // TODO: this
  util::StatusObject
  LinkManager::ExtractStatus() const
  {
    return {};
  }

  void
  LinkManager::init(RCLookupHandler* rcLookup)
  {
    stopping = false;
    _rcLookup = rcLookup;
    _nodedb = router->nodedb();
  }

  void
  LinkManager::connect_to(RouterID router)
  {
    auto fn = [this](
                  const RouterID& rid, const RouterContact* const rc, const RCRequestResult res) {
      if (res == RCRequestResult::Success)
        connect_to(*rc);
      /* TODO:
      else
        RC lookup failure callback here
      */
    };

    _rcLookup->GetRC(router, fn);
  }

  // This function assumes the RC has already had its signature verified and connection is allowed.
  void
  LinkManager::connect_to(RouterContact rc)
  {
    // TODO: connection failed callback
    if (have_connection_to(rc.pubkey))
      return;

    // RC shouldn't be valid if this is the case, but may as well sanity check...
    // TODO: connection failed callback
    if (rc.addrs.empty())
      return;

    // TODO: connection failed callback
    auto* ep = get_compatible_link(rc);
    if (ep == nullptr)
      return;

    // TODO: connection established/failed callbacks
    oxen::quic::stream_data_callback stream_cb =
        [this](oxen::quic::Stream& stream, bstring_view packet) {
          recv_control_message(stream, packet);
        };

    // TODO: once "compatible link" cares about address, actually choose addr to connect to
    //       based on which one is compatible with the link we chose.  For now, just use
    //       the first one.
    auto& selected = rc.addrs[0];
    oxen::quic::opt::remote_addr remote{selected.IPString(), selected.port};
    // TODO: confirm remote end is using the expected pubkey (RouterID).
    // TODO: ALPN for "client" vs "relay" (could just be set on endpoint creation)
    // TODO: does connect() inherit the endpoint's datagram data callback, and do we want it to if
    // so?
    auto conn_interface = ep->endpoint->connect(remote, stream_cb, tls_creds);

    std::shared_ptr<oxen::quic::Stream> stream = conn_interface->get_new_stream();

    llarp::link::Connection conn;
    conn.conn = conn_interface;
    conn.control_stream = stream;
    conn.remote_rc = rc;
    conn.inbound = false;
    conn.remote_is_relay = true;

    ep->connections[rc.pubkey] = std::move(conn);
    ep->connid_map[conn_interface->scid()] = rc.pubkey;
  }

  void
  LinkManager::connect_to_random(int numDesired)
  {
    std::set<RouterID> exclude;
    auto remainingDesired = numDesired;
    do
    {
      auto filter = [exclude](const auto& rc) -> bool { return exclude.count(rc.pubkey) == 0; };

      RouterContact other;
      if (const auto maybe = _nodedb->GetRandom(filter))
      {
        other = *maybe;
      }
      else
        break;

      exclude.insert(other.pubkey);
      if (not _rcLookup->SessionIsAllowed(other.pubkey))
        continue;

      Connect(other);
      --remainingDesired;
    } while (remainingDesired > 0);
  }

  void
  LinkManager::recv_data_message(oxen::quic::dgram_interface&, bstring)
  {
    // TODO: this
  }

  void
  LinkManager::recv_control_message(oxen::quic::Stream&, bstring_view)
  {
    // TODO: this
  }

}  // namespace llarp
