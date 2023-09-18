#include "link_manager.hpp"
#include "connection.hpp"

#include <llarp/router/router.hpp>
#include <llarp/router/rc_lookup_handler.hpp>
#include <llarp/nodedb.hpp>

#include <algorithm>
#include <set>

namespace llarp
{
  namespace link
  {
    std::shared_ptr<link::Connection>
    Endpoint::get_conn(const RouterContact& rc) const
    {
      if (auto itr = conns.find(rc.pubkey); itr != conns.end())
        return itr->second;

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

    bool
    Endpoint::deregister_peer(RouterID remote)
    {
      if (auto itr = conns.find(remote); itr != conns.end())
      {
        itr->second->conn->close_connection();
        conns.erase(itr);
        return true;
      }

      return false;
    }

    size_t
    Endpoint::num_connected(bool clients_only) const
    {
      size_t count = 0;

      for (const auto& c : conns)
      {
        if (not(c.second->remote_is_relay and clients_only))
          count += 1;
      }

      return count;
    }

    bool
    Endpoint::get_random_connection(RouterContact& router) const
    {
      if (const auto size = conns.size(); size)
      {
        auto itr = conns.begin();
        std::advance(itr, randint() % size);
        router = itr->second->remote_rc;
        return true;
      }

      log::warning(quic_cat, "Error: failed to fetch random connection");
      return false;
    }
  }  // namespace link

  // TODO: pass connection open callback to endpoint constructor!
  LinkManager::LinkManager(Router& r)
      : router{r}
      , quic{std::make_unique<oxen::quic::Network>()}
      , tls_creds{oxen::quic::GNUTLSCreds::make_from_ed_keys(
            {reinterpret_cast<const char*>(router.identity().data()), size_t{32}},
            {reinterpret_cast<const char*>(router.identity().toPublic().data()), size_t{32}})}
      , ep{quic->endpoint(
               router.public_ip(),
               [this](oxen::quic::connection_interface& ci) { return on_conn_open(ci); }),
           *this}
  {}

  // TODO: replace with control/data message sending with libquic
  bool
  LinkManager::send_to(const RouterID& remote, bstring data, uint16_t priority)
  {
    if (stopping)
      return false;

    if (not have_connection_to(remote))
    {
      auto pending = PendingMessage(data, priority);

      auto [itr, b] = pending_conn_msg_queue.emplace(remote, MessageQueue());
      itr->second.push(std::move(pending));

      rc_lookup->get_rc(
          remote,
          [this](
              [[maybe_unused]] const RouterID& rid,
              const RouterContact* const rc,
              const RCRequestResult res) {
            if (res == RCRequestResult::Success)
              connect_to(*rc);
            else
              log::warning(quic_cat, "Do something intelligent here for error handling");
          });

      // TODO: some error callback to report message send failure
      // or, should we connect and pass a send-msg callback as the connection successful cb?
      return false;
    }

    // TODO: send the message
    // TODO: if we keep bool return type, change this accordingly

    return false;
  }

  void
  LinkManager::connect_to(RouterID router)
  {
    rc_lookup->get_rc(
        router,
        [this](
            [[maybe_unused]] const RouterID& rid,
            const RouterContact* const rc,
            const RCRequestResult res) {
          if (res == RCRequestResult::Success)
            connect_to(*rc);
          /* TODO:
          else
            RC lookup failure callback here
          */
        });
  }

  // This function assumes the RC has already had its signature verified and connection is allowed.
  void
  LinkManager::connect_to(RouterContact rc)
  {
    if (have_connection_to(rc.pubkey))
    {
      // TODO: connection failed callback
      return;
    }

    // TODO: connection established/failed callbacks
    oxen::quic::stream_data_callback stream_cb =
        [this](oxen::quic::Stream& stream, bstring_view packet) {
          recv_control_message(stream, packet);
        };

    // TODO: once "compatible link" cares about address, actually choose addr to connect to
    //       based on which one is compatible with the link we chose.  For now, just use
    //       the first one.
    auto& remote_addr = rc.addr;

    // TODO: confirm remote end is using the expected pubkey (RouterID).
    // TODO: ALPN for "client" vs "relay" (could just be set on endpoint creation)
    // TODO: does connect() inherit the endpoint's datagram data callback, and do we want it to if
    // so?
    if (auto rv = ep.establish_connection(remote_addr, rc, stream_cb, tls_creds); rv)
    {
      log::info(quic_cat, "Connection to {} successfully established!", remote_addr);
      return;
    }
    log::warning(quic_cat, "Connection to {} successfully established!", remote_addr);
  }

  void
  LinkManager::on_conn_open(oxen::quic::connection_interface& ci)
  {
    router.loop()->call([]() {});

    const auto& scid = ci.scid();
    const auto& rid = ep.connid_map[scid];

    if (auto itr = pending_conn_msg_queue.find(rid); itr != pending_conn_msg_queue.end())
    {
      auto& que = itr->second;

      while (not que.empty())
      {
        auto& m = que.top();

        (m.is_control) ? ep.conns[rid]->control_stream->send(std::move(m.buf))
                       : ci.send_datagram(std::move(m.buf));

        que.pop();
      }
    }
  };

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
      log::info(logcat, "Peer {} successfully de-registered", remote);
    }
    else
      log::warning(logcat, "Peer {} not found for de-registration!", remote);
  }

  void
  LinkManager::stop()
  {
    if (stopping)
    {
      return;
    }

    util::Lock l(m);

    LogInfo("stopping links");
    stopping = true;

    quic.reset();
  }

  void
  LinkManager::set_conn_persist(const RouterID& remote, llarp_time_t until)
  {
    if (stopping)
      return;

    util::Lock l(m);

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
    return ep.num_connected(clients_only);
  }

  size_t
  LinkManager::get_num_connected_clients() const
  {
    return get_num_connected(true);
  }

  bool
  LinkManager::get_random_connected(RouterContact& router) const
  {
    return ep.get_random_connection(router);
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
  LinkManager::extract_status() const
  {
    return {};
  }

  void
  LinkManager::init(RCLookupHandler* rcLookup)
  {
    stopping = false;
    rc_lookup = rcLookup;
    node_db = router.node_db();
  }

  void
  LinkManager::connect_to_random(int num_conns)
  {
    std::set<RouterID> exclude;
    auto remainder = num_conns;

    do
    {
      auto filter = [exclude](const auto& rc) -> bool { return exclude.count(rc.pubkey) == 0; };

      if (auto maybe_other = node_db->GetRandom(filter))
      {
        exclude.insert(maybe_other->pubkey);

        if (not rc_lookup->is_session_allowed(maybe_other->pubkey))
          continue;

        connect_to(*maybe_other);
        --remainder;
      }
    } while (remainder > 0);
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
