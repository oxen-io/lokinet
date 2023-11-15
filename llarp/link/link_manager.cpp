#include "link_manager.hpp"

#include "connection.hpp"
#include "contacts.hpp"

#include <llarp/messages/dht.hpp>
#include <llarp/messages/exit.hpp>
#include <llarp/messages/path.hpp>
#include <llarp/nodedb.hpp>
#include <llarp/path/path.hpp>
#include <llarp/router/router.hpp>

#include <algorithm>
#include <set>

namespace llarp
{
  namespace link
  {
    std::shared_ptr<link::Connection>
    Endpoint::get_conn(const RemoteRC& rc) const
    {
      if (auto itr = conns.find(rc.router_id()); itr != conns.end())
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

        link_manager._router.loop()->call([this, scid = _scid, rid = _rid]() {
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
    Endpoint::get_random_connection(RemoteRC& router) const
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

        link_manager._router.loop()->call([this, scid = _scid, rid = _rid]() {
          endpoint->close_connection(scid);

          conns.erase(rid);
          connid_map.erase(scid);
        });
      }
    }

  }  // namespace link

  using messages::serialize_response;

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
    assert(ep.connid_map.count(s->conn_id()));
    const RouterID& rid = ep.connid_map[s->conn_id()];

    s->register_command("path_build"s, [this, rid](oxen::quic::message m) {
      _router.loop()->call(
          [this, &rid, msg = std::move(m)]() mutable { handle_path_build(std::move(msg), rid); });
    });

    s->register_command("path_control"s, [this, rid](oxen::quic::message m) {
      _router.loop()->call(
          [this, &rid, msg = std::move(m)]() mutable { handle_path_control(std::move(msg), rid); });
    });

    for (auto& method : direct_requests)
    {
      s->register_command(
          std::string{method.first}, [this, func = method.second](oxen::quic::message m) {
            _router.loop()->call([this, msg = std::move(m), func = std::move(func)]() mutable {
              auto body = msg.body_str();
              auto respond = [m = std::move(msg)](std::string response) mutable {
                m.respond(std::move(response));
              };
              std::invoke(func, this, body, std::move(respond));
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
        _router.public_ip(),
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
      : _router{r}
      , quic{std::make_unique<oxen::quic::Network>()}
      , tls_creds{oxen::quic::GNUTLSCreds::make_from_ed_keys(
            {reinterpret_cast<const char*>(_router.identity().data()), size_t{32}},
            {reinterpret_cast<const char*>(_router.identity().toPublic().data()), size_t{32}})}
      , ep{startup_endpoint(), *this}
  {}

  bool
  LinkManager::send_control_message(
      const RouterID& remote,
      std::string endpoint,
      std::string body,
      std::function<void(oxen::quic::message m)> func)
  {
    assert(func);  // makes no sense to send control message and ignore response

    if (func)
    {
      func = [this, f = std::move(func)](oxen::quic::message m) mutable {
        _router.loop()->call([func = std::move(f), msg = std::move(m)]() mutable { func(msg); });
      };
    }

    return send_control_message_impl(remote, std::move(endpoint), std::move(body), std::move(func));
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

    if (auto conn = ep.get_conn(remote); conn)
    {
      conn->control_stream->command(std::move(endpoint), std::move(body), std::move(func));
      return true;
    }

    _router.loop()->call([this,
                          remote,
                          endpoint = std::move(endpoint),
                          body = std::move(body),
                          f = std::move(func)]() {
      auto pending = PendingControlMessage(std::move(body), std::move(endpoint), f);

      auto [itr, b] = pending_conn_msg_queue.emplace(remote, MessageQueue());
      itr->second.push_back(std::move(pending));

      connect_to(remote);
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

    _router.loop()->call([this, body = std::move(body), remote]() {
      auto pending = PendingDataMessage(body);

      auto [itr, b] = pending_conn_msg_queue.emplace(remote, MessageQueue());
      itr->second.push_back(std::move(pending));

      connect_to(remote);
    });

    return false;
  }

  void
  LinkManager::close_connection(RouterID rid)
  {
    return ep.close_connection(rid);
  }

  void
  LinkManager::connect_to(const RouterID& rid)
  {
    auto rc = node_db->get_rc(rid);
    if (rc)
    {
      connect_to(*rc);
    }
    else
      log::warning(quic_cat, "Do something intelligent here for error handling");
  }

  // This function assumes the RC has already had its signature verified and connection is allowed.
  void
  LinkManager::connect_to(const RemoteRC& rc)
  {
    if (auto conn = ep.get_conn(rc.router_id()); conn)
    {
      // TODO: should implement some connection failed logic, but not the same logic that
      // would be executed for another failure case
      return;
    }

    const auto& remote_addr = rc.addr();

    // TODO: confirm remote end is using the expected pubkey (RouterID).
    // TODO: ALPN for "client" vs "relay" (could just be set on endpoint creation)
    if (auto rv = ep.establish_connection(
            oxen::quic::RemoteAddress{rc.router_id().ToView(), remote_addr}, rc);
        rv)
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
    _router.loop()->call([this, &conn_interface = ci]() {
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
    _router.loop()->call([this, &conn_interface = ci, error_code = ec]() {
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

  void
  LinkManager::gossip_rc(const RemoteRC& rc)
  {
    for (auto& [rid, conn] : ep.conns)
      send_control_message(rid, "gossip_rc", std::string{rc.view()}, nullptr);
  }

  void
  LinkManager::handle_gossip_rc(oxen::quic::message m)
  {
    try
    {
      RemoteRC rc{m.body()};
    }
    catch (const std::exception& e)
    {
      log::info(link_cat, "Recieved invalid RC, dropping on the floor.");
      return;
    }
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

    LogInfo("stopping links");
    is_stopping = true;

    quic.reset();
  }

  void
  LinkManager::set_conn_persist(const RouterID& remote, llarp_time_t until)
  {
    if (is_stopping)
      return;

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
  LinkManager::get_random_connected(RemoteRC& router) const
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

  // TODO: this
  util::StatusObject
  LinkManager::extract_status() const
  {
    return {};
  }

  void
  LinkManager::init()
  {
    is_stopping = false;
    node_db = _router.node_db();
  }

  void
  LinkManager::connect_to_random(int num_conns)
  {
    std::set<RouterID> exclude;
    auto remainder = num_conns;

    do
    {
      auto filter = [exclude](const auto& rc) -> bool {
        return exclude.count(rc.router_id()) == 0;
      };

      if (auto maybe_other = node_db->GetRandom(filter))
      {
        exclude.insert(maybe_other->router_id());

        if (not node_db->is_connection_allowed(maybe_other->router_id()))
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
  LinkManager::handle_find_name(std::string_view body, std::function<void(std::string)> respond)
  {
    std::string name_hash;

    try
    {
      oxenc::bt_dict_consumer btdp{body};

      name_hash = btdp.require<std::string>("H");
    }
    catch (const std::exception& e)
    {
      log::warning(link_cat, "Exception: {}", e.what());
      respond(messages::ERROR_RESPONSE);
    }

    _router.rpc_client()->lookup_ons_hash(
        name_hash,
        [respond = std::move(respond)](
            [[maybe_unused]] std::optional<service::EncryptedName> maybe) mutable {
          if (maybe)
            respond(serialize_response({{"NAME", maybe->ciphertext}}));
          else
            respond(serialize_response({{messages::STATUS_KEY, FindNameMessage::NOT_FOUND}}));
        });
  }

  void
  LinkManager::handle_find_name_response(oxen::quic::message m)
  {
    if (m.timed_out)
    {
      log::info(link_cat, "FindNameMessage timed out!");
      return;
    }

    std::string payload;

    try
    {
      oxenc::bt_dict_consumer btdc{m.body()};
      payload = btdc.require<std::string>(m ? "NAME" : messages::STATUS_KEY);
    }
    catch (const std::exception& e)
    {
      log::warning(link_cat, "Exception: {}", e.what());
      return;
    }

    if (m)
    {
      // TODO: wtf
    }
    else
    {
      if (payload == "ERROR")
      {
        log::info(link_cat, "FindNameMessage failed with unkown error!");

        // resend?
      }
      else if (payload == FindNameMessage::NOT_FOUND)
      {
        log::info(link_cat, "FindNameMessage failed with unkown error!");
        // what to do here?
      }
      else
        log::info(link_cat, "FindNameMessage failed with unkown error!");
    }
  }

  void
  LinkManager::handle_publish_intro(std::string_view body, std::function<void(std::string)> respond)
  {
    std::string introset, derived_signing_key, payload, sig, nonce;
    uint64_t is_relayed, relay_order;
    std::chrono::milliseconds signed_at;

    try
    {
      oxenc::bt_dict_consumer btdc_a{body};

      introset = btdc_a.require<std::string>("I");
      relay_order = btdc_a.require<uint64_t>("O");
      is_relayed = btdc_a.require<uint64_t>("R");

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
      respond(messages::ERROR_RESPONSE);
      return;
    }

    const auto now = _router.now();
    const auto addr = dht::Key_t{reinterpret_cast<uint8_t*>(derived_signing_key.data())};
    const auto local_key = _router.rc().router_id();

    if (not service::EncryptedIntroSet::verify(introset, derived_signing_key, sig))
    {
      log::error(link_cat, "Received PublishIntroMessage with invalid introset: {}", introset);
      respond(serialize_response({{messages::STATUS_KEY, PublishIntroMessage::INVALID_INTROSET}}));
      return;
    }

    if (now + service::MAX_INTROSET_TIME_DELTA > signed_at + path::DEFAULT_LIFETIME)
    {
      log::error(link_cat, "Received PublishIntroMessage with expired introset: {}", introset);
      respond(serialize_response({{messages::STATUS_KEY, PublishIntroMessage::EXPIRED}}));
      return;
    }

    auto closest_rcs = _router.node_db()->find_many_closest_to(addr, INTROSET_STORAGE_REDUNDANCY);

    if (closest_rcs.size() != INTROSET_STORAGE_REDUNDANCY)
    {
      log::error(
          link_cat, "Received PublishIntroMessage but only know {} nodes", closest_rcs.size());
      respond(serialize_response({{messages::STATUS_KEY, PublishIntroMessage::INSUFFICIENT}}));
      return;
    }

    service::EncryptedIntroSet enc{derived_signing_key, signed_at, payload, nonce, sig};

    if (is_relayed)
    {
      if (relay_order >= INTROSET_STORAGE_REDUNDANCY)
      {
        log::error(
            link_cat, "Received PublishIntroMessage with invalide relay order: {}", relay_order);
        respond(serialize_response({{messages::STATUS_KEY, PublishIntroMessage::INVALID_ORDER}}));
        return;
      }

      log::info(link_cat, "Relaying PublishIntroMessage for {}", addr);

      const auto& peer_rc = closest_rcs[relay_order];
      const auto& peer_key = peer_rc.router_id();

      if (peer_key == local_key)
      {
        log::info(
            link_cat,
            "Received PublishIntroMessage in which we are peer index {}.. storing introset",
            relay_order);

        _router.contacts()->services()->PutNode(dht::ISNode{std::move(enc)});
        respond(serialize_response({{messages::STATUS_KEY, ""}}));
      }
      else
      {
        log::info(
            link_cat, "Received PublishIntroMessage; propagating to peer index {}", relay_order);

        send_control_message(
            peer_key,
            "publish_intro",
            PublishIntroMessage::serialize(introset, relay_order, is_relayed),
            [respond = std::move(respond)](oxen::quic::message m) {
              if (m.timed_out)
                return;  // drop if timed out; requester will have timed out as well
              respond(m.body_str());
            });
      }

      return;
    }

    int rc_index = -1, index = 0;

    for (const auto& rc : closest_rcs)
    {
      if (rc.router_id() == local_key)
      {
        rc_index = index;
        break;
      }
      ++index;
    }

    if (rc_index >= 0)
    {
      log::info(link_cat, "Received PublishIntroMessage for {} (TXID: {}); we are candidate {}");

      _router.contacts()->services()->PutNode(dht::ISNode{std::move(enc)});
      respond(serialize_response({{messages::STATUS_KEY, ""}}));
    }
    else
      log::warning(
          link_cat,
          "Received non-relayed PublishIntroMessage from {}; we are not the candidate",
          addr);
  }

  void
  LinkManager::handle_publish_intro_response(oxen::quic::message m)
  {
    if (m.timed_out)
    {
      log::info(link_cat, "PublishIntroMessage timed out!");
      return;
    }

    std::string payload;

    try
    {
      oxenc::bt_dict_consumer btdc{m.body()};
      payload = btdc.require<std::string>(messages::STATUS_KEY);
    }
    catch (const std::exception& e)
    {
      log::warning(link_cat, "Exception: {}", e.what());
      return;
    }

    if (m)
    {
      // DISCUSS: not sure what to do on success of a publish intro command?
    }
    else
    {
      if (payload == "ERROR")
      {
        log::info(link_cat, "PublishIntroMessage failed with remote exception!");
        // Do something smart here probably
        return;
      }

      log::info(link_cat, "PublishIntroMessage failed with error code: {}", payload);

      if (payload == PublishIntroMessage::INVALID_INTROSET)
      {}
      else if (payload == PublishIntroMessage::EXPIRED)
      {}
      else if (payload == PublishIntroMessage::INSUFFICIENT)
      {}
      else if (payload == PublishIntroMessage::INVALID_ORDER)
      {}
    }
  }

  void
  LinkManager::handle_find_intro(std::string_view body, std::function<void(std::string)> respond)
  {
    ustring location;
    uint64_t relay_order, is_relayed;

    try
    {
      oxenc::bt_dict_consumer btdc{body};

      relay_order = btdc.require<uint64_t>("O");
      is_relayed = btdc.require<uint64_t>("R");
      location = btdc.require<ustring>("S");
    }
    catch (const std::exception& e)
    {
      log::warning(link_cat, "Exception: {}", e.what());
      respond(messages::ERROR_RESPONSE);
      return;
    }

    const auto addr = dht::Key_t{location.data()};

    if (is_relayed)
    {
      if (relay_order >= INTROSET_STORAGE_REDUNDANCY)
      {
        log::warning(
            link_cat, "Received FindIntroMessage with invalid relay order: {}", relay_order);
        respond(serialize_response({{messages::STATUS_KEY, FindIntroMessage::INVALID_ORDER}}));
        return;
      }

      auto closest_rcs = _router.node_db()->find_many_closest_to(addr, INTROSET_STORAGE_REDUNDANCY);

      if (closest_rcs.size() != INTROSET_STORAGE_REDUNDANCY)
      {
        log::error(
            link_cat, "Received FindIntroMessage but only know {} nodes", closest_rcs.size());
        respond(serialize_response({{messages::STATUS_KEY, FindIntroMessage::INSUFFICIENT_NODES}}));
        return;
      }

      log::info(link_cat, "Relaying FindIntroMessage for {}", addr);

      const auto& peer_rc = closest_rcs[relay_order];
      const auto& peer_key = peer_rc.router_id();

      send_control_message(
          peer_key,
          "find_intro",
          FindIntroMessage::serialize(dht::Key_t{peer_key}, is_relayed, relay_order),
          [respond = std::move(respond)](oxen::quic::message relay_response) mutable {
            if (relay_response)
              log::info(
                  link_cat,
                  "Relayed FindIntroMessage returned successful response; transmitting to initial "
                  "requester");
            else if (relay_response.timed_out)
              log::critical(
                  link_cat, "Relayed FindIntroMessage timed out! Notifying initial requester");
            else
              log::critical(
                  link_cat, "Relayed FindIntroMessage failed! Notifying initial requester");

            respond(relay_response.body_str());
          });
    }
    else
    {
      if (auto maybe_intro = _router.contacts()->get_introset_by_location(addr))
        respond(serialize_response({{"INTROSET", maybe_intro->bt_encode()}}));
      else
      {
        log::warning(
            link_cat,
            "Received FindIntroMessage with relayed == false and no local introset entry");
        respond(serialize_response({{messages::STATUS_KEY, FindIntroMessage::NOT_FOUND}}));
      }
    }
  }

  void
  LinkManager::handle_find_intro_response(oxen::quic::message m)
  {
    if (m.timed_out)
    {
      log::info(link_cat, "FindIntroMessage timed out!");
      return;
    }

    std::string payload;

    try
    {
      oxenc::bt_dict_consumer btdc{m.body()};
      payload = btdc.require<std::string>((m) ? "INTROSET" : messages::STATUS_KEY);
    }
    catch (const std::exception& e)
    {
      log::warning(link_cat, "Exception: {}", e.what());
      return;
    }

    // success case, neither timed out nor errored
    if (m)
    {
      service::EncryptedIntroSet enc{payload};
      _router.contacts()->services()->PutNode(std::move(enc));
    }
    else
    {
      log::info(link_cat, "FindIntroMessage failed with error: {}", payload);
      // Do something smart here probably
    }
  }

  void
  LinkManager::handle_path_build(oxen::quic::message m, const RouterID& from)
  {
    if (!_router.path_context().AllowingTransit())
    {
      log::warning(link_cat, "got path build request when not permitting transit");
      m.respond(serialize_response({{messages::STATUS_KEY, PathBuildMessage::NO_TRANSIT}}), true);
      return;
    }

    try
    {
      auto payload_list = oxenc::bt_deserialize<std::deque<ustring>>(m.body());
      if (payload_list.size() != path::MAX_LEN)
      {
        log::info(link_cat, "Path build message with wrong number of frames");
        m.respond(serialize_response({{messages::STATUS_KEY, PathBuildMessage::BAD_FRAMES}}), true);
        return;
      }

      oxenc::bt_dict_consumer frame_info{payload_list.front()};
      auto hash = frame_info.require<ustring>("HASH");
      auto frame = frame_info.require<ustring>("FRAME");

      oxenc::bt_dict_consumer hop_dict{frame};
      auto hop_payload = hop_dict.require<ustring>("ENCRYPTED");
      auto outer_nonce = hop_dict.require<ustring>("NONCE");
      auto other_pubkey = hop_dict.require<ustring>("PUBKEY");

      SharedSecret shared;
      // derive shared secret using ephemeral pubkey and our secret key (and nonce)
      if (!crypto::dh_server(
              shared.data(), other_pubkey.data(), _router.pubkey(), outer_nonce.data()))
      {
        log::info(link_cat, "DH server initialization failed during path build");
        m.respond(serialize_response({{messages::STATUS_KEY, PathBuildMessage::BAD_CRYPTO}}), true);
        return;
      }

      // hash data and check against given hash
      ShortHash digest;
      if (!crypto::hmac(digest.data(), frame.data(), frame.size(), shared))
      {
        log::error(link_cat, "HMAC failed on path build request");
        m.respond(serialize_response({{messages::STATUS_KEY, PathBuildMessage::BAD_CRYPTO}}), true);
        return;
      }
      if (!std::equal(digest.begin(), digest.end(), hash.data()))
      {
        log::info(link_cat, "HMAC mismatch on path build request");
        m.respond(serialize_response({{messages::STATUS_KEY, PathBuildMessage::BAD_CRYPTO}}), true);
        return;
      }

      // decrypt frame with our hop info
      if (!crypto::xchacha20(
              hop_payload.data(), hop_payload.size(), shared.data(), outer_nonce.data()))
      {
        log::info(link_cat, "Decrypt failed on path build request");
        m.respond(serialize_response({{messages::STATUS_KEY, PathBuildMessage::BAD_CRYPTO}}), true);
        return;
      }

      oxenc::bt_dict_consumer hop_info{hop_payload};
      auto commkey = hop_info.require<std::string>("COMMKEY");
      auto lifetime = hop_info.require<uint64_t>("LIFETIME");
      auto inner_nonce = hop_info.require<ustring>("NONCE");
      auto rx_id = hop_info.require<std::string>("RX");
      auto tx_id = hop_info.require<std::string>("TX");
      auto upstream = hop_info.require<std::string>("UPSTREAM");

      // populate transit hop object with hop info
      // TODO: IP / path build limiting clients
      auto hop = std::make_shared<path::TransitHop>();
      hop->info.downstream = from;

      // extract pathIDs and check if zero or used
      hop->info.txID.from_string(tx_id);
      hop->info.rxID.from_string(rx_id);

      if (hop->info.txID.IsZero() || hop->info.rxID.IsZero())
      {
        log::warning(link_cat, "Invalid PathID; PathIDs must be non-zero");
        m.respond(serialize_response({{messages::STATUS_KEY, PathBuildMessage::BAD_PATHID}}), true);
        return;
      }

      hop->info.upstream.from_string(upstream);

      if (_router.path_context().HasTransitHop(hop->info))
      {
        log::warning(link_cat, "Invalid PathID; PathIDs must be unique");
        m.respond(serialize_response({{messages::STATUS_KEY, PathBuildMessage::BAD_PATHID}}), true);
        return;
      }

      if (!crypto::dh_server(
              hop->pathKey.data(), other_pubkey.data(), _router.pubkey(), inner_nonce.data()))
      {
        log::warning(link_cat, "DH failed during path build.");
        m.respond(serialize_response({{messages::STATUS_KEY, PathBuildMessage::BAD_CRYPTO}}), true);
        return;
      }
      // generate hash of hop key for nonce mutation
      ShortHash xor_hash;
      crypto::shorthash(xor_hash, hop->pathKey.data(), hop->pathKey.size());
      hop->nonceXOR = xor_hash.data();  // nonceXOR is 24 bytes, ShortHash is 32; this will truncate

      // set and check path lifetime
      hop->lifetime = 1ms * lifetime;

      if (hop->lifetime >= path::DEFAULT_LIFETIME)
      {
        log::warning(link_cat, "Path build attempt with too long of a lifetime.");
        m.respond(
            serialize_response({{messages::STATUS_KEY, PathBuildMessage::BAD_LIFETIME}}), true);
        return;
      }

      hop->started = _router.now();
      _router.persist_connection_until(hop->info.downstream, hop->ExpireTime() + 10s);

      if (hop->info.upstream == _router.pubkey())
      {
        hop->terminal_hop = true;
        // we are terminal hop and everything is okay
        _router.path_context().PutTransitHop(hop);
        m.respond(messages::OK_RESPONSE, false);
        return;
      }

      // pop our frame, to be randomized after onion step and appended
      auto end_frame = std::move(payload_list.front());
      payload_list.pop_front();
      auto onion_nonce = SymmNonce{inner_nonce.data()} ^ hop->nonceXOR;
      // (de-)onion each further frame using the established shared secret and
      // onion_nonce = inner_nonce ^ nonceXOR
      // Note: final value passed to crypto::onion is xor factor, but that's for *after* the
      // onion round to compute the return value, so we don't care about it.
      for (auto& element : payload_list)
      {
        crypto::onion(element.data(), element.size(), hop->pathKey, onion_nonce, onion_nonce);
      }
      // randomize final frame.  could probably paste our frame on the end and onion it with the
      // rest, but it gains nothing over random.
      randombytes(end_frame.data(), end_frame.size());
      payload_list.push_back(std::move(end_frame));

      send_control_message(
          hop->info.upstream,
          "path_build",
          oxenc::bt_serialize(payload_list),
          [hop, this, prev_message = std::move(m)](oxen::quic::message m) {
            if (m)
            {
              log::info(
                  link_cat,
                  "Upstream returned successful path build response; giving hop info to Router, "
                  "then relaying response");
              _router.path_context().PutTransitHop(hop);
            }
            if (m.timed_out)
              log::info(link_cat, "Upstream timed out on path build; relaying timeout");
            else
              log::info(link_cat, "Upstream returned path build failure; relaying response");

            m.respond(m.body_str(), not m);
          });
    }
    catch (const std::exception& e)
    {
      log::warning(link_cat, "Exception: {}", e.what());
      m.respond(messages::ERROR_RESPONSE, true);
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
      m.respond(messages::ERROR_RESPONSE, true);
      return;
    }
  }

  void
  LinkManager::handle_path_latency_response(oxen::quic::message m)
  {
    try
    {
      oxenc::bt_dict_consumer btdc{m.body()};
    }
    catch (const std::exception& e)
    {
      log::warning(link_cat, "Exception: {}", e.what());
      // m.respond(serialize_response({{messages::STATUS_KEY, "EXCEPTION"}}), true);
      return;
    }
  }

  void
  LinkManager::handle_path_transfer(oxen::quic::message m)
  {
    try
    {
      oxenc::bt_dict_consumer btdc{m.body()};
    }
    catch (const std::exception& e)
    {
      log::warning(link_cat, "Exception: {}", e.what());
      m.respond(messages::ERROR_RESPONSE, true);
      return;
    }
  }

  void
  LinkManager::handle_path_transfer_response(oxen::quic::message m)
  {
    try
    {
      oxenc::bt_dict_consumer btdc{m.body()};
    }
    catch (const std::exception& e)
    {
      log::warning(link_cat, "Exception: {}", e.what());
      m.respond(messages::ERROR_RESPONSE, true);
      return;
    }
  }

  void
  LinkManager::handle_obtain_exit(oxen::quic::message m)
  {
    try
    {
      uint64_t flag;
      ustring_view pubkey, sig;
      std::string_view tx_id;

      oxenc::bt_list_consumer btlc{m.body()};
      auto dict_data = btlc.consume_dict_data();
      oxenc::bt_dict_consumer btdc{dict_data};

      sig = to_usv(btlc.consume_string_view());
      flag = btdc.require<uint64_t>("E");
      pubkey = btdc.require<ustring_view>("I");
      tx_id = btdc.require<std::string_view>("T");

      RouterID target{pubkey.data()};
      auto transit_hop =
          _router.path_context().GetTransitHop(target, PathID_t{to_usv(tx_id).data()});

      const auto rx_id = transit_hop->info.rxID;

      auto success =
          (crypto::verify(pubkey, to_usv(dict_data), sig)
           and _router.exitContext().ObtainNewExit(PubKey{pubkey.data()}, rx_id, flag != 0));

      m.respond(
          ObtainExitMessage::sign_and_serialize_response(_router.identity(), tx_id), not success);
    }
    catch (const std::exception& e)
    {
      log::warning(link_cat, "Exception: {}", e.what());
      m.respond(messages::ERROR_RESPONSE, true);
      throw;
    }
  }

  void
  LinkManager::handle_obtain_exit_response(oxen::quic::message m)
  {
    if (m.timed_out)
    {
      log::info(link_cat, "ObtainExitMessage timed out!");
      return;
    }
    if (m.is_error)
    {
      // TODO: what to do here
    }

    try
    {
      std::string_view tx_id;
      ustring_view sig;

      oxenc::bt_list_consumer btlc{m.body()};
      auto dict_data = btlc.consume_dict_data();
      oxenc::bt_dict_consumer btdc{dict_data};

      sig = to_usv(btlc.consume_string_view());
      tx_id = btdc.require<std::string_view>("T");

      auto path_ptr = _router.path_context().GetPath(PathID_t{to_usv(tx_id).data()});

      if (crypto::verify(_router.pubkey(), to_usv(dict_data), sig))
        path_ptr->enable_exit_traffic();
    }
    catch (const std::exception& e)
    {
      log::warning(link_cat, "Exception: {}", e.what());
      throw;
    }
  }

  void
  LinkManager::handle_update_exit(oxen::quic::message m)
  {
    try
    {
      std::string_view path_id, tx_id;
      ustring_view sig;

      oxenc::bt_list_consumer btlc{m.body()};
      auto dict_data = btlc.consume_dict_data();
      oxenc::bt_dict_consumer btdc{dict_data};

      sig = to_usv(btlc.consume_string_view());
      path_id = btdc.require<std::string_view>("P");
      tx_id = btdc.require<std::string_view>("T");

      auto transit_hop =
          _router.path_context().GetTransitHop(_router.pubkey(), PathID_t{to_usv(tx_id).data()});

      if (auto exit_ep =
              _router.exitContext().FindEndpointForPath(PathID_t{to_usv(path_id).data()}))
      {
        if (crypto::verify(exit_ep->PubKey().data(), to_usv(dict_data), sig))
        {
          (exit_ep->UpdateLocalPath(transit_hop->info.rxID))
              ? m.respond(UpdateExitMessage::sign_and_serialize_response(_router.identity(), tx_id))
              : m.respond(
                  serialize_response({{messages::STATUS_KEY, UpdateExitMessage::UPDATE_FAILED}}),
                  true);
        }
        // If we fail to verify the message, no-op
      }
    }
    catch (const std::exception& e)
    {
      log::warning(link_cat, "Exception: {}", e.what());
      m.respond(messages::ERROR_RESPONSE, true);
      return;
    }
  }

  void
  LinkManager::handle_update_exit_response(oxen::quic::message m)
  {
    if (m.timed_out)
    {
      log::info(link_cat, "UpdateExitMessage timed out!");
      return;
    }
    if (m.is_error)
    {
      // TODO: what to do here
    }

    try
    {
      std::string tx_id;
      ustring_view sig;

      oxenc::bt_list_consumer btlc{m.body()};
      auto dict_data = btlc.consume_dict_data();
      oxenc::bt_dict_consumer btdc{dict_data};

      sig = to_usv(btlc.consume_string_view());
      tx_id = btdc.require<std::string_view>("T");

      auto path_ptr = _router.path_context().GetPath(PathID_t{to_usv(tx_id).data()});

      if (crypto::verify(_router.pubkey(), to_usv(dict_data), sig))
      {
        if (path_ptr->update_exit(std::stoul(tx_id)))
        {
          // TODO: talk to tom and Jason about how this stupid shit was a no-op originally
          // see Path::HandleUpdateExitVerifyMessage
        }
        else
        {}
      }
    }
    catch (const std::exception& e)
    {
      log::warning(link_cat, "Exception: {}", e.what());
      return;
    }
  }

  void
  LinkManager::handle_close_exit(oxen::quic::message m)
  {
    try
    {
      std::string_view tx_id;
      ustring_view sig;

      oxenc::bt_list_consumer btlc{m.body()};
      auto dict_data = btlc.consume_dict_data();
      oxenc::bt_dict_consumer btdc{dict_data};

      sig = to_usv(btlc.consume_string_view());
      tx_id = btdc.require<std::string_view>("T");

      auto transit_hop =
          _router.path_context().GetTransitHop(_router.pubkey(), PathID_t{to_usv(tx_id).data()});

      const auto rx_id = transit_hop->info.rxID;

      if (auto exit_ep = router().exitContext().FindEndpointForPath(rx_id))
      {
        if (crypto::verify(exit_ep->PubKey().data(), to_usv(dict_data), sig))
        {
          exit_ep->Close();
          m.respond(CloseExitMessage::sign_and_serialize_response(_router.identity(), tx_id));
        }
      }

      m.respond(
          serialize_response({{messages::STATUS_KEY, CloseExitMessage::UPDATE_FAILED}}), true);
    }
    catch (const std::exception& e)
    {
      log::warning(link_cat, "Exception: {}", e.what());
      m.respond(messages::ERROR_RESPONSE, true);
      return;
    }
  }

  void
  LinkManager::handle_close_exit_response(oxen::quic::message m)
  {
    if (m.timed_out)
    {
      log::info(link_cat, "CloseExitMessage timed out!");
      return;
    }
    if (m.is_error)
    {
      // TODO: what to do here
    }

    try
    {
      std::string_view nonce, tx_id;
      ustring_view sig;

      oxenc::bt_list_consumer btlc{m.body()};
      auto dict_data = btlc.consume_dict_data();
      oxenc::bt_dict_consumer btdc{dict_data};

      sig = to_usv(btlc.consume_string_view());
      tx_id = btdc.require<std::string_view>("T");
      nonce = btdc.require<std::string_view>("Y");

      auto path_ptr = _router.path_context().GetPath(PathID_t{to_usv(tx_id).data()});

      if (path_ptr->SupportsAnyRoles(path::ePathRoleExit | path::ePathRoleSVC)
          and crypto::verify(_router.pubkey(), to_usv(dict_data), sig))
        path_ptr->mark_exit_closed();
    }
    catch (const std::exception& e)
    {
      log::warning(link_cat, "Exception: {}", e.what());
      return;
    }
  }

  void
  LinkManager::handle_path_control(oxen::quic::message m, const RouterID& from)
  {
    try
    {
      oxenc::bt_dict_consumer btdc{m.body()};
      auto nonce = SymmNonce{btdc.require<ustring_view>("NONCE").data()};
      auto path_id_str = btdc.require<ustring_view>("PATHID");
      auto payload = btdc.require<std::string>("PAYLOAD");
      auto path_id = PathID_t{path_id_str.data()};
      auto hop = _router.path_context().GetTransitHop(from, path_id);

      // TODO: use "path_control" for both directions?  If not, drop message on
      // floor if we don't have the path_id in question; if we decide to make this
      // bidirectional, will need to check if we have a Path with path_id.
      if (not hop)
        return;

      // if terminal hop, payload should contain a request (e.g. "find_name"); handle and respond.
      if (hop->terminal_hop)
      {
        hop->onion(payload, nonce, false);
        handle_inner_request(std::move(m), std::move(payload), std::move(hop));
        return;
      }

      auto next_id = path_id == hop->info.rxID ? hop->info.txID : hop->info.rxID;
      auto next_router = path_id == hop->info.rxID ? hop->info.upstream : hop->info.downstream;
      auto new_payload = hop->onion_and_payload(payload, next_id, nonce);
      send_control_message(
          next_router,
          "path_control"s,
          std::move(new_payload),
          [hop_weak = hop->weak_from_this(), path_id, prev_message = std::move(m)](
              oxen::quic::message response) mutable {
            auto hop = hop_weak.lock();
            if (not hop)
              return;

            oxenc::bt_dict_consumer resp_btdc{response.body()};
            auto nonce = SymmNonce{resp_btdc.require<ustring_view>("NONCE").data()};
            auto payload = resp_btdc.require<std::string>("PAYLOAD");
            auto resp_payload = hop->onion_and_payload(payload, path_id, nonce);
            prev_message.respond(std::move(resp_payload), false);
          });
    }
    catch (const std::exception& e)
    {
      log::warning(link_cat, "Exception: {}", e.what());
      return;
    }
  }

  void
  LinkManager::handle_inner_request(
      oxen::quic::message m, std::string payload, std::shared_ptr<path::TransitHop> hop)
  {
    oxenc::bt_dict_consumer btdc{payload};
    auto body = btdc.require<std::string_view>("BODY");
    auto method = btdc.require<std::string_view>("METHOD");

    // If a handler exists for "method", call it; else drop request on the floor.
    auto itr = path_requests.find(method);
    if (itr == path_requests.end())
    {
      log::info(link_cat, "Received path control request \"{}\", which has no handler.", method);
      return;
    }

    auto respond = [m = std::move(m),
                    hop_weak = hop->weak_from_this()](std::string response) mutable {
      auto hop = hop_weak.lock();
      if (not hop)
        return;  // transit hop gone, drop response

      m.respond(hop->onion_and_payload(response, hop->info.rxID), false);
    };

    std::invoke(itr->second, this, std::move(body), std::move(respond));
  }

  void
  LinkManager::handle_convo_intro(oxen::quic::message m)
  {
    if (m.timed_out)
    {
      log::info(link_cat, "Path control message timed out!");
      return;
    }

    try
    {}
    catch (const std::exception& e)
    {
      log::warning(link_cat, "Exception: {}", e.what());
      return;
    }
  }

}  // namespace llarp
