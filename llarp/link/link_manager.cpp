#include "link_manager.hpp"
#include "connection.hpp"
#include "contacts.hpp"

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

    std::shared_ptr<link::Connection>
    Endpoint::get_conn(const RouterID& rid) const
    {
      if (auto itr = conns.find(rid); itr != conns.end())
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
    Endpoint::deregister_peer(RouterID _rid)
    {
      if (auto itr = conns.find(_rid); itr != conns.end())
      {
        auto& c = itr->second;
        auto& _scid = c->conn->scid();

        link_manager.router.loop()->call([this, scid = _scid, rid = _rid]() {
          endpoint->close_connection(scid);

          conns.erase(rid);
          connid_map.erase(scid);
        });

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

    void
    Endpoint::for_each_connection(std::function<void(link::Connection&)> func)
    {
      for (const auto& [rid, conn] : conns)
        func(*conn);
    }

    void
    Endpoint::close_connection(RouterID _rid)
    {
      if (auto itr = conns.find(_rid); itr != conns.end())
      {
        auto& c = itr->second;
        auto& _scid = c->conn->scid();

        link_manager.router.loop()->call([this, scid = _scid, rid = _rid]() {
          endpoint->close_connection(scid);

          conns.erase(rid);
          connid_map.erase(scid);
        });
      }
    }

  }  // namespace link

  void
  LinkManager::for_each_connection(std::function<void(link::Connection&)> func)
  {
    if (is_stopping)
      return;

    return ep.for_each_connection(func);
  }

  void
  LinkManager::register_commands(std::shared_ptr<oxen::quic::BTRequestStream>& s)
  {
    for (const auto& [name, func] : rpc_commands)
    {
      s->register_command(name, [this, f = func](oxen::quic::message m) {
        router.loop()->call([this, func = f, msg = std::move(m)]() mutable {
          std::invoke(func, this, std::move(msg));
        });
      });
    }
  }

  std::shared_ptr<oxen::quic::Endpoint>
  LinkManager::startup_endpoint()
  {
    /** Parameters:
          - local bind address
          - conection open callback
          - connection close callback
          - stream constructor callback
            - will return a BTRequestStream on the first call to get_new_stream<BTRequestStream>
    */
    auto ep = quic->endpoint(
        router.public_ip(),
        [this](oxen::quic::connection_interface& ci) { return on_conn_open(ci); },
        [this](oxen::quic::connection_interface& ci, uint64_t ec) {
          return on_conn_closed(ci, ec);
        },
        [this](oxen::quic::dgram_interface& di, bstring dgram) { recv_data_message(di, dgram); });
    ep->listen(
        tls_creds,
        [&](oxen::quic::Connection& c,
            oxen::quic::Endpoint& e,
            std::optional<int64_t> id) -> std::shared_ptr<oxen::quic::Stream> {
          if (id && id == 0)
          {
            auto s = std::make_shared<oxen::quic::BTRequestStream>(c, e);
            register_commands(s);
            return s;
          }
          return std::make_shared<oxen::quic::Stream>(c, e);
        });
    return ep;
  }

  LinkManager::LinkManager(Router& r)
      : router{r}
      , quic{std::make_unique<oxen::quic::Network>()}
      , tls_creds{oxen::quic::GNUTLSCreds::make_from_ed_keys(
            {reinterpret_cast<const char*>(router.identity().data()), size_t{32}},
            {reinterpret_cast<const char*>(router.identity().toPublic().data()), size_t{32}})}
      , ep{startup_endpoint(), *this}
  {}

  bool
  LinkManager::send_control_message(
      const RouterID& remote,
      std::string endpoint,
      std::string body,
      std::function<void(oxen::quic::message m)> func)
  {
    if (func)
      return send_control_message_impl(
          remote, std::move(endpoint), std::move(body), std::move(func));

    if (auto itr = rpc_responses.find(endpoint); itr != rpc_responses.end())
      return send_control_message_impl(
          remote, std::move(endpoint), std::move(body), [&](oxen::quic::message m) {
            return std::invoke(itr->second, this, std::move(m));
          });

    return send_control_message_impl(remote, std::move(endpoint), std::move(body));
  }

  bool
  LinkManager::send_control_message_impl(
      const RouterID& remote,
      std::string endpoint,
      std::string body,
      std::function<void(oxen::quic::message m)> func)
  {
    if (is_stopping)
      return false;

    auto cb = [this, f = std::move(func), endpoint](oxen::quic::message m) {
      f(m);

      if (auto itr = rpc_responses.find(endpoint); itr != rpc_responses.end())
        std::invoke(itr->second, this, std::move(m));
    };

    if (auto conn = ep.get_conn(remote); conn)
    {
      conn->control_stream->command(endpoint, body, std::move(func));
      return true;
    }

    router.loop()->call([&]() {
      auto pending = PendingControlMessage(body, endpoint, func);

      auto [itr, b] = pending_conn_msg_queue.emplace(remote, MessageQueue());
      itr->second.push_back(std::move(pending));

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
    });

    return false;
  }

  bool
  LinkManager::send_data_message(const RouterID& remote, std::string body)
  {
    if (is_stopping)
      return false;

    if (auto conn = ep.get_conn(remote); conn)
    {
      conn->conn->send_datagram(std::move(body));
      return true;
    }

    router.loop()->call([&]() {
      auto pending = PendingDataMessage(body);

      auto [itr, b] = pending_conn_msg_queue.emplace(remote, MessageQueue());
      itr->second.push_back(std::move(pending));

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
    });

    return false;
  }

  void
  LinkManager::close_connection(RouterID rid)
  {
    return ep.close_connection(rid);
  }

  void
  LinkManager::connect_to(RouterID rid)
  {
    rc_lookup->get_rc(
        rid,
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
    if (auto conn = ep.get_conn(rc.pubkey); conn)
    {
      // TODO: should implement some connection failed logic, but not the same logic that
      // would be executed for another failure case
      return;
    }

    auto& remote_addr = rc.addr;

    // TODO: confirm remote end is using the expected pubkey (RouterID).
    // TODO: ALPN for "client" vs "relay" (could just be set on endpoint creation)
    if (auto rv = ep.establish_connection(remote_addr, rc); rv)
    {
      log::info(quic_cat, "Connection to {} successfully established!", remote_addr);
      return;
    }
    log::warning(quic_cat, "Connection to {} successfully established!", remote_addr);
  }

  // TODO: should we add routes here now that Router::SessionOpen is gone?
  void
  LinkManager::on_conn_open(oxen::quic::connection_interface& ci)
  {
    router.loop()->call([this, &conn_interface = ci]() {
      const auto& scid = conn_interface.scid();
      const auto& rid = ep.connid_map[scid];

      // check to see if this connection was established while we were attempting to queue
      // messages to the remote
      if (auto itr = pending_conn_msg_queue.find(rid); itr != pending_conn_msg_queue.end())
      {
        auto& que = itr->second;

        while (not que.empty())
        {
          auto& m = que.front();

          if (m.is_control)
          {
            auto& msg = reinterpret_cast<PendingControlMessage&>(m);
            ep.conns[rid]->control_stream->command(msg.endpoint, msg.body, msg.func);
          }
          else
          {
            auto& msg = reinterpret_cast<PendingDataMessage&>(m);
            conn_interface.send_datagram(std::move(msg.body));
          }

          que.pop_front();
        }
      }
    });
  };

  void
  LinkManager::on_conn_closed(oxen::quic::connection_interface& ci, uint64_t ec)
  {
    router.loop()->call([this, &conn_interface = ci, error_code = ec]() {
      const auto& scid = conn_interface.scid();

      log::debug(quic_cat, "Purging quic connection CID:{} (ec: {})", scid, error_code);

      if (const auto& c_itr = ep.connid_map.find(scid); c_itr != ep.connid_map.end())
      {
        const auto& rid = c_itr->second;

        if (auto p_itr = pending_conn_msg_queue.find(rid); p_itr != pending_conn_msg_queue.end())
          pending_conn_msg_queue.erase(p_itr);

        if (auto m_itr = ep.conns.find(rid); m_itr != ep.conns.end())
          ep.conns.erase(m_itr);

        ep.connid_map.erase(c_itr);

        log::debug(quic_cat, "Quic connection CID:{} purged successfully", scid);
      }
    });
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
      log::info(logcat, "Peer {} successfully de-registered", remote);
    }
    else
      log::warning(logcat, "Peer {} not found for de-registration!", remote);
  }

  void
  LinkManager::stop()
  {
    if (is_stopping)
    {
      return;
    }

    util::Lock l(m);

    LogInfo("stopping links");
    is_stopping = true;

    quic.reset();
  }

  void
  LinkManager::set_conn_persist(const RouterID& remote, llarp_time_t until)
  {
    if (is_stopping)
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
    if (is_stopping)
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
    is_stopping = false;
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
  LinkManager::handle_find_name(oxen::quic::message m)
  {
    std::string name_hash;
    [[maybe_unused]] uint64_t tx_id;

    try
    {
      oxenc::bt_dict_consumer btdp{m.body()};

      name_hash = btdp.require<std::string>("H");
      tx_id = btdp.require<uint64_t>("T");
    }
    catch (const std::exception& e)
    {
      log::warning(link_cat, "Exception: {}", e.what());
      m.respond("ERROR", true);
    }

    router.rpc_client()->lookup_ons_hash(
        name_hash, [this, msg = std::move(m)](std::optional<service::EncryptedName> maybe) mutable {
          if (maybe.has_value())
            msg.respond(serialize_response(true, {{"NAME", maybe->ciphertext.c_str()}}));
          else
            msg.respond(serialize_response(false, {{"STATUS", "NOT FOUND"}}), true);
        });
  }

  std::string
  LinkManager::serialize_response(bool success, oxenc::bt_dict supplement)
  {
    return oxenc::bt_serialize(oxenc::bt_list{(success) ? 1 : 0, supplement});
  }

  void
  LinkManager::handle_find_router(oxen::quic::message m)
  {
    std::string target_key;
    uint64_t is_exploratory, is_iterative;

    try
    {
      oxenc::bt_dict_consumer btdc{m.body()};

      is_exploratory = btdc.require<uint64_t>("E");
      is_iterative = btdc.require<uint64_t>("I");
      target_key = btdc.require<std::string>("E");
    }
    catch (const std::exception& e)
    {
      log::warning(link_cat, "Exception: {}", e.what());
      m.respond(serialize_response(false, {{"STATUS", "EXCEPTION"}}), true);
      return;
    }

    // TODO: do we need a replacement for dht.AllowTransit() etc here?

    // TODO: do we need a replacement for dht.pendingIntroSetLookups() etc here?

    RouterID target_rid;
    target_rid.FromString(target_key);
    const auto target_addr = dht::Key_t{reinterpret_cast<uint8_t*>(target_key.data())};
    const auto& local_rid = router.rc().pubkey;
    const auto local_key = dht::Key_t{local_rid};

    if (is_exploratory)
    {
      std::string neighbors{};
      const auto closest_rcs =
          router.node_db()->FindManyClosestTo(target_addr, RC_LOOKUP_STORAGE_REDUNDANCY);

      for (const auto& rc : closest_rcs)
      {
        const auto& rid = rc.pubkey;
        if (router.router_profiling().IsBadForConnect(rid) || target_rid == rid || local_rid == rid)
          continue;

        neighbors += oxenc::bt_serialize(rid.ToString());
      }

      m.respond(
          serialize_response(
              false, {{"STATUS", "RETRY EXPLORATORY"}, {"ROUTERS", neighbors.c_str()}}),
          true);
    }
    else
    {
      const auto closest_rc = router.node_db()->FindClosestTo(target_addr);
      const auto& closest_rid = closest_rc.pubkey;
      const auto closest_key = dht::Key_t{closest_rid};

      if (target_addr == closest_key)
      {
        if (closest_rc.ExpiresSoon(llarp::time_now_ms()))
        {
          send_control_message_impl(
              target_rid, "find_router", m.body_str(), [this](oxen::quic::message m) {
                return handle_find_router_response(std::move(m));
              });
        }
        else
        {
          m.respond(serialize_response(true, {{"RC", closest_rc.ToString().c_str()}}));
        }
      }
      else if (not is_iterative)
      {
        if ((closest_key ^ target_addr) < (local_key ^ target_addr))
        {
          send_control_message_impl(
              closest_rc.pubkey, "find_router", m.body_str(), [this](oxen::quic::message m) {
                return handle_find_router_response(std::move(m));
              });
        }
        else
        {
          m.respond(serialize_response(false, {{"STATUS", "RETRY ITERATIVE"}}), true);
        }
      }
      else
      {
        m.respond(
            serialize_response(
                false,
                {{"STATUS", "RETRY NEW RECIPIENT"},
                 {"RECIPIENT", reinterpret_cast<const char*>(closest_rid.data())}}),
            true);
      }
    }
  }

  void
  LinkManager::handle_publish_intro(oxen::quic::message m)
  {
    std::string introset, derived_signing_key, payload, sig, nonce;
    uint64_t is_relayed, relay_order, tx_id;
    std::chrono::milliseconds signed_at;

    try
    {
      oxenc::bt_dict_consumer btdc_a{m.body()};

      introset = btdc_a.require<std::string>("I");
      relay_order = btdc_a.require<uint64_t>("O");
      is_relayed = btdc_a.require<uint64_t>("R");
      tx_id = btdc_a.require<uint64_t>("T");

      oxenc::bt_dict_consumer btdc_b{introset.data()};

      derived_signing_key = btdc_b.require<std::string>("d");
      nonce = btdc_b.require<std::string>("n");
      signed_at = std::chrono::milliseconds{btdc_b.require<uint64_t>("s")};
      payload = btdc_b.require<std::string>("x");
      sig = btdc_b.require<std::string>("z");
    }
    catch (const std::exception& e)
    {
      log::warning(link_cat, "Exception: {}", e.what());
      m.respond(serialize_response(false, {{"STATUS", "EXCEPTION"}}), true);
      return;
    }

    const auto now = router.now();
    const auto addr = dht::Key_t{reinterpret_cast<uint8_t*>(derived_signing_key.data())};
    const auto local_key = router.rc().pubkey;

    if (not service::EncryptedIntroSet::verify(introset, derived_signing_key, sig))
    {
      log::error(link_cat, "Received PublishIntroMessage with invalid introset: {}", introset);
      m.respond(serialize_response(false, {{"STATUS", "INVALID INTROSET"}}), true);
      return;
    }

    if (now + service::MAX_INTROSET_TIME_DELTA > signed_at + path::DEFAULT_LIFETIME)
    {
      log::error(link_cat, "Received PublishIntroMessage with expired introset: {}", introset);
      m.respond(serialize_response(false, {{"STATUS", "EXPIRED INTROSET"}}), true);
      return;
    }

    auto closest_rcs = router.node_db()->FindManyClosestTo(addr, INTROSET_STORAGE_REDUNDANCY);

    if (closest_rcs.size() != INTROSET_STORAGE_REDUNDANCY)
    {
      log::error(
          link_cat, "Received PublishIntroMessage but only know {} nodes", closest_rcs.size());
      m.respond(serialize_response(false, {{"STATUS", "INSUFFICIENT NODES"}}), true);
      return;
    }

    service::EncryptedIntroSet enc{derived_signing_key, signed_at, payload, nonce, sig};

    if (is_relayed)
    {
      if (relay_order >= INTROSET_STORAGE_REDUNDANCY)
      {
        log::error(
            link_cat, "Received PublishIntroMessage with invalide relay order: {}", relay_order);
        m.respond(serialize_response(false, {{"STATUS", "INVALID ORDER"}}), true);
        return;
      }

      log::info(link_cat, "Relaying PublishIntroMessage for {} (TXID: {})", addr, tx_id);

      const auto& peer_rc = closest_rcs[relay_order];
      const auto& peer_key = peer_rc.pubkey;

      if (peer_key == local_key)
      {
        log::info(
            link_cat,
            "Received PublishIntroMessage in which we are peer index {}.. storing introset",
            relay_order);

        router.contacts()->services()->PutNode(dht::ISNode{std::move(enc)});
        m.respond(serialize_response(true));
      }
      else
      {
        log::info(
            link_cat, "Received PublishIntroMessage; propagating to peer index {}", relay_order);

        send_control_message_impl(
            peer_key, "publish_intro", m.body_str(), [this](oxen::quic::message m) {
              return handle_publish_intro_response(std::move(m));
            });
      }

      return;
    }

    int rc_index = -1, index = 0;

    for (const auto& rc : closest_rcs)
    {
      if (rc.pubkey == local_key)
      {
        rc_index = index;
        break;
      }
      ++index;
    }

    if (rc_index >= 0)
    {
      log::info(link_cat, "Received PublishIntroMessage for {} (TXID: {}); we are candidate {}");

      router.contacts()->services()->PutNode(dht::ISNode{std::move(enc)});
      m.respond(serialize_response(true));
    }
    else
      log::warning(
          link_cat,
          "Received non-relayed PublishIntroMessage from {}; we are not the candidate",
          addr);
  }

  void
  LinkManager::handle_find_intro(oxen::quic::message m)
  {
    std::string tag_name, location;
    uint64_t tx_id, relay_order, is_relayed;

    try
    {
      oxenc::bt_dict_consumer btdc{m.body()};

      tag_name = btdc.require<std::string>("N");
      relay_order = btdc.require<uint64_t>("O");
      is_relayed = btdc.require<uint64_t>("R");
      location = btdc.require<std::string>("S");
      tx_id = btdc.require<uint64_t>("T");
    }
    catch (const std::exception& e)
    {
      log::warning(link_cat, "Exception: {}", e.what());
      m.respond(serialize_response(false, {{"STATUS", "EXCEPTION"}}), true);
      return;
    }

    // TODO: do we need a replacement for dht.pendingIntroSetLookups() etc here?

    const auto addr = dht::Key_t{reinterpret_cast<uint8_t*>(location.data())};

    if (is_relayed)
    {
      if (relay_order >= INTROSET_STORAGE_REDUNDANCY)
      {
        log::warning(
            link_cat, "Received FindIntroMessage with invalid relay order: {}", relay_order);
        m.respond(serialize_response(false, {{"STATUS", "INVALID ORDER"}}), true);
        return;
      }

      auto closest_rcs = router.node_db()->FindManyClosestTo(addr, INTROSET_STORAGE_REDUNDANCY);

      if (closest_rcs.size() != INTROSET_STORAGE_REDUNDANCY)
      {
        log::error(
            link_cat, "Received FindIntroMessage but only know {} nodes", closest_rcs.size());
        m.respond(serialize_response(false, {{"STATUS", "INSUFFICIENT NODES"}}), true);
        return;
      }

      log::info(link_cat, "Relaying FindIntroMessage for {} (TXID: {})", addr, tx_id);

      const auto& peer_rc = closest_rcs[relay_order];
      const auto& peer_key = peer_rc.pubkey;

      send_control_message_impl(
          peer_key, "find_intro", m.body_str(), [this](oxen::quic::message m) {
            return handle_find_intro_response(std::move(m));
          });
      return;
    }

    // TODO: replace this concept and add it to the response
    // const auto maybe = dht.GetIntroSetByLocation(location);
    m.respond(serialize_response(true, {{"INTROSET", ""}}));
  }

  void
  LinkManager::handle_path_confirm(oxen::quic::message m)
  {
    try
    {
      oxenc::bt_dict_consumer btdc{m.body()};
    }
    catch (const std::exception& e)
    {
      log::warning(link_cat, "Exception: {}", e.what());
      m.respond(serialize_response(false, {{"STATUS", "EXCEPTION"}}), true);
      return;
    }
  }

  void
  LinkManager::handle_path_latency(oxen::quic::message m)
  {
    try
    {
      oxenc::bt_dict_consumer btdc{m.body()};
    }
    catch (const std::exception& e)
    {
      log::warning(link_cat, "Exception: {}", e.what());
      m.respond(serialize_response(false, {{"STATUS", "EXCEPTION"}}), true);
      return;
    }
  }

  void
  LinkManager::handle_update_exit(oxen::quic::message m)
  {
    try
    {
      oxenc::bt_dict_consumer btdc{m.body()};
    }
    catch (const std::exception& e)
    {
      log::warning(link_cat, "Exception: {}", e.what());
      m.respond(serialize_response(false, {{"STATUS", "EXCEPTION"}}), true);
      return;
    }
  }

  void
  LinkManager::handle_obtain_exit(oxen::quic::message m)
  {
    // TODO: implement transit_hop things like nextseqno(), info.rxID, etc
    std::string payload{m.body_str()}, pubkey;
    [[maybe_unused]] uint64_t flag, tx_id, seq_no;

    try
    {
      oxenc::bt_dict_consumer btdc{payload};

      flag = btdc.require<uint64_t>("E");
      pubkey = btdc.require<std::string>("I");
      seq_no = btdc.require<uint64_t>("S");
      tx_id = btdc.require<uint64_t>("T");
    }
    catch (const std::exception& e)
    {
      log::warning(link_cat, "Exception: {}", e.what());
      m.respond(serialize_response(false, {{"STATUS", "EXCEPTION"}}), true);
      return;
    }

    RouterID target;
    target.FromString(pubkey);

    // auto handler = router.path_context().GetByDownstream(target, tx_id);
  }

  void
  LinkManager::handle_close_exit(oxen::quic::message m)
  {
    try
    {
      oxenc::bt_dict_consumer btdc{m.body()};
    }
    catch (const std::exception& e)
    {
      log::warning(link_cat, "Exception: {}", e.what());
      m.respond(serialize_response(false, {{"STATUS", "EXCEPTION"}}), true);
      return;
    }
  }

  void
  LinkManager::handle_publish_intro_response(oxen::quic::message m)
  {
    try
    {
      oxenc::bt_dict_consumer btdc{m.body()};
    }
    catch (const std::exception& e)
    {
      log::warning(link_cat, "Exception: {}", e.what());
      m.respond(serialize_response(false, {{"STATUS", "EXCEPTION"}}), true);
      return;
    }
  }

  void
  LinkManager::handle_find_name_response(oxen::quic::message m)
  {
    try
    {
      oxenc::bt_dict_consumer btdc{m.body()};
    }
    catch (const std::exception& e)
    {
      log::warning(link_cat, "Exception: {}", e.what());
      m.respond(serialize_response(false, {{"STATUS", "EXCEPTION"}}), true);
      return;
    }
  }

  void
  LinkManager::handle_find_router_response(oxen::quic::message m)
  {
    try
    {
      oxenc::bt_dict_consumer btdc{m.body()};
    }
    catch (const std::exception& e)
    {
      log::warning(link_cat, "Exception: {}", e.what());
      m.respond(serialize_response(false, {{"STATUS", "EXCEPTION"}}), true);
      return;
    }
  }

  void
  LinkManager::handle_find_intro_response(oxen::quic::message m)
  {
    try
    {
      oxenc::bt_dict_consumer btdc{m.body()};
    }
    catch (const std::exception& e)
    {
      log::warning(link_cat, "Exception: {}", e.what());
      m.respond(serialize_response(false, {{"STATUS", "EXCEPTION"}}), true);
      return;
    }

    // check if we have any pending intro lookups in contacts
  }
}  // namespace llarp
