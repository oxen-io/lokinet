#include "session.hpp"

#include <llarp/crypto/crypto.hpp>
#include <llarp/handlers/session.hpp>
#include <llarp/link/tunnel.hpp>
#include <llarp/messages/dht.hpp>
#include <llarp/messages/path.hpp>
#include <llarp/messages/session.hpp>
#include <llarp/router/router.hpp>
#include <llarp/util/formattable.hpp>

#include <utility>

namespace llarp::session
{
    static auto logcat = log::Cat("session");

    BaseSession::BaseSession(
        Router& r,
        std::shared_ptr<session_path_interface> _p,
        handlers::SessionEndpoint& parent,
        NetworkAddress remote,
        HopID remote_pivot_txid,
        session_tag _t,
        bool use_tun,
        bool is_outbound,
        shared_kx_data kx_data)
        : _r{r},
          _parent{parent},
          _tag{std::move(_t)},
          _remote{std::move(remote)},
          session_keys{std::make_unique<shared_kx_data>(std::move(kx_data))},
          _remote_pivot_txid{std::move(remote_pivot_txid)},
          _use_tun{use_tun},
          _is_outbound{is_outbound},
          _is_snode_session{_is_outbound ? !_remote.is_client() : _r.is_service_node()},
          _is_exit_session{_tag.proto_bits().first}
    {
        set_new_current_path_interface(std::move(_p));

        if (_use_tun)
            _recv_dgram = [this](std::vector<uint8_t> data) {
                _r.tun_endpoint()->handle_inbound_packet(IPPacket{std::move(data)}, _tag, _remote);
            };
        else
            _recv_dgram = [this](std::vector<uint8_t> data) {
                _ep->manually_receive_packet(
                    NetworkPacket{oxen::quic::Path{}, bstring{reinterpret_cast<std::byte*>(data.data()), data.size()}});
            };
    }

    BaseSession::~BaseSession()
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);
        deactivate();

        if (_current_path and _current_path->is_linked())
            _current_path->unlink_session(_tag);
    }

    bool BaseSession::send_path_control_message(std::string method, std::string body, bt_control_response_hook func)
    {
        auto inner_payload = PATH::CONTROL::serialize(std::move(method), std::move(body));
        auto intermediate_payload = PATH::CONTROL::serialize_aligned(std::move(inner_payload), _remote_pivot_txid);

        return _current_path->send_path_control_message(
            "path_control", std::move(intermediate_payload), std::move(func));
    }

    bool BaseSession::send_path_data_message(std::string data)
    {
        log::debug(logcat, "{} called", __PRETTY_FUNCTION__);
        session_keys->encrypt(data);

        auto inner_payload = PATH::DATA::serialize_inner(std::move(data), _tag);
        auto intermediate_payload = PATH::DATA::serialize_intermediate(std::move(inner_payload), _remote_pivot_txid);

        return _current_path->send_path_data_message(std::move(intermediate_payload));
    }

    void BaseSession::recv_path_data_message(std::vector<uint8_t> data)
    {
        session_keys->decrypt(data);

        if (_recv_dgram)
            _recv_dgram(std::move(data));
        else
            throw std::runtime_error{"Session does not have hook to receive datagrams!"};
    }

    void BaseSession::set_new_current_path_interface(std::shared_ptr<session_path_interface> _new_path)
    {
        if (_current_path)
            _current_path->unlink_session(_tag);

        _current_path = std::move(_new_path);
        _pivot_txid = _current_path->terminal_txid();

        _current_path->link_session(_tag);
        assert(_current_path->is_linked());

        log::debug(logcat, "Session to remote ({}) set new current path {}", _remote, _current_path->to_string());
    }

    void BaseSession::set_remote_pivot_tx(HopID new_remote_txid)
    {
        log::debug(logcat, "Setting new pivot txID for remote ({}) [ new:{} ]", _remote, new_remote_txid);
        _remote_pivot_txid = std::move(new_remote_txid);
    }

    void BaseSession::publish_client_contact(const EncryptedClientContact& ecc, bt_control_response_hook func)
    {
        send_path_control_message(
            "publish_cc", PublishClientContact::serialize(std::move(ecc), _r.local_rid()), std::move(func));
    }

    void BaseSession::_init_ep()
    {
        _ep = _r.quic_tunnel()->net()->endpoint(
            LOCALHOST_BLANK, oxen::quic::opt::manual_routing{[this](const oxen::quic::Path&, bstring_view data) {
                send_path_data_message(std::string{reinterpret_cast<const char*>(data.data()), data.size()});
            }});
    }

    void BaseSession::tcp_backend_connect()
    {
        _init_ep();

        // TODO: change the libquic address to the lokinet-primary-ip:port (or just the ip)
        auto _handle = TCPHandle::make_client(_r.loop(), oxen::quic::Address{});

        _ep->listen(
            _r.quic_tunnel()->creds(),
            [this](oxen::quic::connection_interface& ci) mutable {
                if (not _ci)
                    _ci = ci.shared_from_this();
                else
                    log::warning(logcat, "Tunneled QUIC endpoint can only have one connection per remote!");
            },
            [this, h = _handle](oxen::quic::Stream& s) mutable {
                // On stream creation, the call to ::connect(...) will:
                //  - create a bufferevent
                //  - set the recv_data_cb in the Stream to write to that bufferevent
                //  - make a TCP connection over the bufferevent to lokinet-primary-ip:port
                auto tcp_conn = h->connect(s.shared_from_this());
                _tcp_conns.insert(std::move(tcp_conn));
                return 0;
            });

        _handles.emplace(_handle->port(), std::move(_handle));
    }

    void BaseSession::tcp_backend_listen(on_session_init_hook cb, uint16_t port)
    {
        _init_ep();

        auto _handle = TCPHandle::make_server(
            _r.loop(),
            [this](struct bufferevent* _bev, evutil_socket_t _fd) mutable {
                auto s = _ci->open_stream<oxen::quic::Stream>([_bev](oxen::quic::Stream& s, bstring_view data) {
                    auto rv = bufferevent_write(_bev, data.data(), data.size());

                    log::info(
                        logcat,
                        "Stream (id:{}) {} {}B to TCP buffer",
                        s.stream_id(),
                        rv < 0 ? "failed to write" : "successfully wrote",
                        data.size());
                });

                auto tcp_conn = std::make_shared<TCPConnection>(_bev, _fd, std::move(s));

                auto [itr, b] = _tcp_conns.insert(std::move(tcp_conn));

                return itr->get();
            },
            port);

        auto bind = _handle->bind();

        if (not bind.has_value())
            throw std::runtime_error{"Failed to bind TCP listener!"};

        _handles.emplace(_handle->port(), std::move(_handle));

        _ci = _ep->connect(
            KeyedAddress{TUNNEL_PUBKEY},
            _r.quic_tunnel()->creds(),
            [addr = *bind, hook = std::move(cb)](oxen::quic::connection_interface&) { hook(addr.to_ipv4()); },
            [](oxen::quic::connection_interface&, uint64_t) {
                // TESTNET: TODO:
            });
    }

    void BaseSession::activate()
    {
        _is_active = true;
        log::debug(logcat, "Session to remote ({}) activated!", _remote);
    }

    void BaseSession::deactivate()
    {
        _is_active = false;
        log::debug(logcat, "Session to remote ({}) deactivated!", _remote);
    }

    void BaseSession::stop_session(bool send_close, bt_control_response_hook func)
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);

        deactivate();

        if (send_close)
        {
            std::promise<void> prom;

            _r.loop()->call([&]() mutable {
                send_path_close(std::move(func));
                prom.set_value();
            });

            prom.get_future().get();
            log::debug(logcat, "Dispatched path close message!");
        }

        _parent.unmap_session(_remote, _use_tun);
    }

    static void session_close_cb(oxen::quic::message m)
    {
        log::debug(logcat, "Remote {} session", m ? "successfully closed" : "failed to close");
    }

    void BaseSession::send_path_close(bt_control_response_hook func)
    {
        log::debug(logcat, "Dispatching close session message...");
        send_path_control_message(
            "session_close", CloseSession::serialize(_tag), func ? std::move(func) : session_close_cb);
    }

    std::string BaseSession::to_string() const
    {
        return "{}BSession:[ active:{} | exit:{} | {} ]"_format(
            detail::bool_alpha(_is_outbound, "O", "I"),
            detail::bool_alpha(_is_active),
            detail::bool_alpha(_is_exit_session),
            _current_path->to_string());
    }

    OutboundRelaySession::OutboundRelaySession(
        NetworkAddress remote,
        handlers::SessionEndpoint& parent,
        std::shared_ptr<path::Path> path,
        session_tag _t,
        HopID remote_pivot_txid,
        shared_kx_data kx_data)
        : PathHandler{parent._router, path::DEFAULT_PATHS_HELD},
          BaseSession(
              _router,
              std::move(path),
              parent,
              std::move(remote),
              std::move(remote_pivot_txid),
              std::move(_t),
              _router.using_tun_if(),
              true,
              std::move(kx_data)),
          _last_use{_router.now()}
    {
        add_path(current_path());

        _path_rotater = _router.loop()->call_every(path::PATH_ROTATION_INTERVAL, [this]() mutable { rotate_paths(); });
    }

    std::shared_ptr<path::Path> OutboundRelaySession::current_path()
    {
        return std::dynamic_pointer_cast<path::Path>(_current_path);
    }

    std::shared_ptr<OutboundRelaySession> OutboundRelaySession::downcast(const std::shared_ptr<BaseSession>& b)
    {
        return std::dynamic_pointer_cast<OutboundRelaySession>(b);
    }

    std::shared_ptr<path::PathHandler> OutboundRelaySession::get_self() { return shared_from_this(); }

    std::weak_ptr<path::PathHandler> OutboundRelaySession::get_weak() { return weak_from_this(); }

    bool OutboundRelaySession::send_path_control_message(
        std::string method, std::string body, bt_control_response_hook func)
    {
        return _current_path->send_path_control_message(std::move(method), std::move(body), std::move(func));
    }

    bool OutboundRelaySession::send_path_data_message(std::string data)
    {
        log::debug(logcat, "{} called", __PRETTY_FUNCTION__);
        // session_keys->encrypt(data);

        // return BaseSession::send_path_data_message(std::move(data));
        // return _current_path->send_path_data_message(std::move(data));
        session_keys->encrypt(data);

        auto inner_payload = PATH::DATA::serialize_inner(std::move(data), _tag);
        auto intermediate_payload = PATH::DATA::serialize_intermediate(std::move(inner_payload), _remote_pivot_txid);

        return _current_path->send_path_data_message(std::move(intermediate_payload));
    }

    void OutboundRelaySession::rotate_paths()
    {
        log::debug(logcat, "{} called", __PRETTY_FUNCTION__);

        Lock_t l{paths_mutex};

        auto rid = _current_path->terminal_rid();

        for (int i = 0; i < SESSION_PATH_BUILD_ATTEMPTS; ++i)
        {
            auto maybe_hops = aligned_hops_to_remote(rid, {}, false);

            if (maybe_hops)
                return path::PathHandler::rotate_paths(std::move(*maybe_hops));

            log::warning(logcat, "Failed attempt #{} to get hops for path-build to remote: {}", i + 1, rid);
        }
    }

    void OutboundRelaySession::select_new_current()
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);
        switch_to_new_path(get_newest_path());
    }

    void OutboundRelaySession::switch_to_new_path(std::shared_ptr<path::Path> p)
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);

        set_remote_pivot_tx(p->pivot_txid());
        set_new_current_path_interface(std::move(p));
        send_path_switch();
    }

    void OutboundRelaySession::drop_oldest_path()
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);
        Lock_t l{paths_mutex};

        auto oldest = get_oldest_path();
        log::debug(logcat, "Dropping oldest path: {}", oldest->to_string());

        bool set_new_current = oldest == _current_path;

        drop_path(oldest);

        if (set_new_current)
            select_new_current();
        else
            log::debug(logcat, "Dropped oldest path; current path is still valid...");
    }

    void OutboundRelaySession::build_more(size_t n)
    {
        size_t count{0};
        log::critical(
            logcat, "OutboundRelaySession building {} paths to remote to have a minimum of {}", n, num_paths_desired);

        while (count < n)
            count += build_path_aligned_to_remote(_current_path->terminal_rid());

        if (count == n)
            log::debug(logcat, "SessionEndpoint successfully initiated {} path-builds", n);
        else
            log::warning(logcat, "SessionEndpoint only initiated {} path-builds (needed: {})", count, n);
    }

    void OutboundRelaySession::send_path_switch()
    {
        log::debug(logcat, "Dispatching path-switch request to remote ({}): {}", _remote, _current_path->to_string());
        send_path_control_message(
            "path_switch",
            SessionPathSwitch::serialize(_tag, _current_path->terminal_txid(), _remote_pivot_txid),
            [](oxen::quic::message m) {
                if (m)
                    log::info(logcat, "Session path switch was successful!");
                else
                {
                    std::optional<std::string> status = std::nullopt;
                    try
                    {
                        oxenc::bt_dict_consumer btdc{m.body()};

                        if (auto s = btdc.maybe<std::string>(messages::STATUS_KEY))
                            status = s;
                    }
                    catch (const std::exception& e)
                    {
                        log::warning(logcat, "Exception: {}", e.what());
                    }

                    log::critical(logcat, "Session path switch FAILED; reason: {}", status.value_or("<none given>"));
                }
            });
    }

    void OutboundRelaySession::stop(bool send_close)
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);
        stop_session(send_close);
    }

    void OutboundRelaySession::stop_session(bool send_close, bt_control_response_hook func)
    {
        log::debug(logcat, "{} called", __PRETTY_FUNCTION__);

        _running = false;

        if (_path_rotater)
        {
            if (_path_rotater->is_running())
                _path_rotater->stop();

            _path_rotater.reset();
            log::trace(logcat, "Path rotation ticker stopped!");
        }

        std::vector<HopID> droplist{};
        {
            Lock_t l{paths_mutex};
            std::ranges::for_each(_paths, [&droplist](auto p) {
                droplist.emplace_back(p.second->upstream_rxid());
                droplist.emplace_back(p.second->pivot_txid());
            });
            log::debug(logcat, "Session droplist holds {} paths", droplist.size());
        }

        _router.loop()->call_soon([wparent = _parent.get_weak(), droplist = std::move(droplist)]() mutable {
            if (auto parent = wparent.lock())
                parent->router().path_context()->drop_paths(std::move(droplist));
            else
                log::warning(logcat, "SessionEndpoint died before dropping session paths");
        });

        BaseSession::stop_session(send_close, std::move(func));
        path::PathHandler::stop();
    }

    OutboundClientSession::OutboundClientSession(
        NetworkAddress remote,
        handlers::SessionEndpoint& parent,
        std::shared_ptr<path::Path> path,
        HopID remote_pivot_txid,
        session_tag _t,
        intro_set _remote_intros,
        shared_kx_data kx_data)
        : OutboundRelaySession{
            std::move(remote), parent, std::move(path), std::move(_t), std::move(remote_pivot_txid), std::move(kx_data)}
    {
        // These can both be false but CANNOT both be true
        if (_is_exit_session and _is_snode_session)
            throw std::runtime_error{"Cannot create OutboundSession for a remote exit and remote service node!"};

        // _path_rotater = _router.loop()->call_every(path::PATH_ROTATION_INTERVAL, [this]() mutable { rotate_paths();
        // }); add_path(current_path());

        populate_intro_map(std::move(_remote_intros));

        log::debug(
            logcat,
            "OutboundSession to remote {} {} created...",
            _is_snode_session ? "relay" : "client",
            _is_exit_session ? "exit" : "service");
    }

    std::shared_ptr<OutboundClientSession> OutboundClientSession::downcast(const std::shared_ptr<BaseSession>& b)
    {
        return std::dynamic_pointer_cast<OutboundClientSession>(b);
    }

    std::shared_ptr<path::PathHandler> OutboundClientSession::get_self() { return shared_from_this(); }

    std::weak_ptr<path::PathHandler> OutboundClientSession::get_weak() { return weak_from_this(); }

    bool OutboundClientSession::send_path_control_message(
        std::string method, std::string body, bt_control_response_hook func)
    {
        return BaseSession::send_path_control_message(std::move(method), std::move(body), std::move(func));
    }

    bool OutboundClientSession::send_path_data_message(std::string data)
    {
        return BaseSession::send_path_data_message(std::move(data));
    }

    void OutboundClientSession::populate_intro_map(const intro_set& _remote_intros)
    {
        log::trace(logcat, "Populating intro map for {} intros!", _remote_intros.size());
        Lock_t l(paths_mutex);

        intro_path_mapping.clear();

        for (auto& intro : _remote_intros)
        {
            log::critical(logcat, "intro: {}", intro);
            if (intro.pivot_txid == _remote_pivot_txid)
                intro_path_mapping.emplace(intro, path::PathPtrSet{current_path()});
            else
                intro_path_mapping.emplace(intro, path::PathPtrSet{});
        }
    }

    void OutboundClientSession::update_remote_intros(intro_set&& intros)
    {
        log::debug(logcat, "Updating ClientIntros for OutboundSession to remote: {}", _remote);
        /**
            - Clear intro_path_map
            - Check path_handler map for any paths to the new intro_set pivots
                - If so:
                    - add to new intro_path_map
                    - make new current path
                - Else:
                    - build and switch
         */

        populate_intro_map(intros);

        if (not update_local_paths())
        {
            log::debug(logcat, "No valid paths left for new intros; building new paths...");
            build_and_switch_paths(std::move(intros));
        }
    }

    bool OutboundClientSession::update_local_paths()
    {
        Lock_t l(paths_mutex);

        bool find_new_current = false;

        for (auto it = _paths.begin(); it != _paths.end();)
        {
            bool keep_path = false;
            const auto& remote_pivot = it->second->pivot_rid();

            for (auto& [intro, pathset] : intro_path_mapping)
            {
                if (intro.pivot_rid == remote_pivot)
                {
                    pathset.emplace(it->second);
                    keep_path = true;
                    break;
                }
            }

            if (keep_path)
                ++it;
            else
            {
                // log::debug(logcat, "Unlinking and dropping path {}", *it->second);
                log::debug(logcat, "Unlinking and dropping path {}", it->second->to_string());
                // it->second->unlink_session(_tag);
                _router.path_context()->drop_path(it->second);

                if (it->second == _current_path)
                {
                    log::debug(logcat, "Dropped path is current session path");
                    find_new_current = true;
                }

                it = _paths.erase(it);
            }
        }

        //  If _paths is empty, we need to build a new current to switch to. Otherwise, if we
        //  had to drop our current path, we need to try to select a new one from the non-empty
        //  _paths map.
        return _paths.empty() ? false : find_new_current ? select_new_current() : true;
    }

    void OutboundClientSession::switch_to_new_path(std::shared_ptr<path::Path> p, HopID new_pivot_txid)
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);

        set_remote_pivot_tx(std::move(new_pivot_txid));
        set_new_current_path_interface(std::move(p));
        send_path_switch();
    }

    bool OutboundClientSession::select_new_current()
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);

        for (auto& [intro, pathset] : intro_path_mapping)
        {
            if (pathset.empty())
                continue;

            for (auto& p : pathset)
            {
                if (p != _current_path)
                {
                    switch_to_new_path(p, intro.pivot_txid);
                    return true;
                }
            }
        }

        log::debug(logcat, "Failed to find valid new current path in intro path mapping...");
        return false;
    }

    nlohmann::json OutboundClientSession::ExtractStatus() const
    {
        auto obj = path::PathHandler::ExtractStatus();
        obj["lastExitUse"] = to_json(_last_use);
        // auto pub = _auth->session_key().to_pubkey();
        // obj["exitIdentity"] = pub.to_string();
        obj["endpoint"] = _remote.to_string();
        return obj;
    }

    void OutboundClientSession::map_path(const std::shared_ptr<path::Path>& p)
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);

        for (auto& [intro, pathset] : intro_path_mapping)
        {
            if (intro.pivot_rid == p->pivot_rid())
            {
                pathset.emplace(p);
                log::debug(logcat, "Client intro {} has {} paths to remote pivot", intro, pathset.size());
                return;
            }
        }

        log::warning(logcat, "Could not match currently held intros to path over pivot ({})", p->pivot_rid());
    }

    bool OutboundClientSession::unmap_path(const std::shared_ptr<path::Path>& p)
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);

        for (auto& [intro, pathset] : intro_path_mapping)
        {
            if (intro.pivot_rid == p->pivot_rid())
            {
                // p->unlink_session(_tag);
                pathset.erase(p);
                log::debug(logcat, "Client intro {} has {} paths to remote pivot", intro, pathset.size());
                return true;
            }
        }

        log::warning(logcat, "Could not match currently held intros to path over pivot ({})", p->pivot_rid());
        return false;
    }

    void OutboundClientSession::path_build_succeeded(std::shared_ptr<path::Path> p)
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);
        Lock_t l(paths_mutex);

        map_path(p);
        path::PathHandler::path_build_succeeded(p);
    }

    void OutboundClientSession::path_build_failed(std::shared_ptr<path::Path> p, bool timeout)
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);
        Lock_t l(paths_mutex);

        unmap_path(p);
        path::PathHandler::path_build_failed(p, timeout);
    }

    void OutboundClientSession::stop_session(bool send_close, bt_control_response_hook func)
    {
        log::debug(logcat, "{} called", __PRETTY_FUNCTION__);

        intro_path_mapping.clear();

        OutboundRelaySession::stop_session(send_close, std::move(func));
    }

    void OutboundClientSession::build_more(size_t n)
    {
        size_t count{0};

        log::critical(
            logcat,
            "OutboundSession building {} paths to have a minimum of {} ({} intros held)",
            n,
            path::DEFAULT_PATHS_HELD,
            intro_path_mapping.size());

        for (auto& [intro, pathset] : intro_path_mapping)
        {
            if (pathset.size() >= PATHS_PER_INTRO)
            {
                log::trace(
                    logcat, "OutboundSession already holds {} paths for intro to pivot {}", pathset.size(), intro);
                continue;
            }

            auto needed = PATHS_PER_INTRO - pathset.size();
            for (size_t i = 0; i < needed && count < n; ++i)
                count += build_path_aligned_to_remote(intro.pivot_rid);

            log::trace(
                logcat,
                "OutboundSession built {} path(s) to have {} for intro to pivot {}",
                needed,
                PATHS_PER_INTRO,
                intro);

            if (count >= n)
                break;
        }

        if (count >= n)
            log::debug(logcat, "OutboundSession successfully initiated {} path-builds", count);
        else
            log::warning(logcat, "OutboundSession only initiated {} path-builds (needed: {})", count, n);
    }

    void OutboundClientSession::build_and_switch_paths()
    {
        log::debug(logcat, "{} called", __PRETTY_FUNCTION__);

        auto intros = get_local_client_intros();
        assert(not intros.empty());

        return build_and_switch_paths(std::move(intros));
    }

    void OutboundClientSession::build_and_switch_paths(intro_set intros)
    {
        log::debug(logcat, "{} called", __PRETTY_FUNCTION__);

        path_build_recursive(
            intros,
            _remote,
            [this](std::shared_ptr<path::Path> new_path, ClientIntro intro) mutable {
                path_build_succeeded(new_path);
                return switch_to_new_path(std::move(new_path), std::move(intro.pivot_txid));
            },
            true);
    }

    void OutboundClientSession::drop_oldest_path()
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);
        Lock_t l{paths_mutex};

        auto oldest = get_oldest_path();
        log::debug(logcat, "Dropping oldest path: {}", oldest->to_string());

        bool found = unmap_path(oldest), set_new_current = oldest == _current_path;

        log::debug(logcat, "{}uccessfully dropped oldest path from OutboundSession mapping", found ? "S" : "Uns");

        drop_path(oldest);

        if (not set_new_current)
        {
            log::debug(logcat, "Dropped oldest path; current path is still valid...");
            return;
        }

        if (select_new_current())
            return;

        build_and_switch_paths();
    }

    void OutboundClientSession::rotate_paths()
    {
        log::debug(logcat, "{} called", __PRETTY_FUNCTION__);

        Lock_t l{paths_mutex};

        // use the newest intro we have for the remote, and build a path to that pivot
        const auto& rid = intro_path_mapping.begin()->first.pivot_rid;

        for (int i = 0; i < SESSION_PATH_BUILD_ATTEMPTS; ++i)
        {
            auto maybe_hops = aligned_hops_to_remote(rid, {}, false);

            if (maybe_hops)
                return path::PathHandler::rotate_paths(std::move(*maybe_hops));

            log::warning(logcat, "Failed attempt #{} to get hops for path-build to remote: {}", i + 1, rid);
        }
    }

    bool OutboundClientSession::is_ready() const
    {
        if (_pivot_txid.is_zero())
            return false;

        const size_t expect = (1 + (num_paths_desired / 2));

        return num_active_paths() >= expect;
    }

    bool OutboundClientSession::is_expired(std::chrono::milliseconds now) const
    {
        return now > _last_use && now - _last_use > path::DEFAULT_LIFETIME;
    }

    InboundClientSession::InboundClientSession(
        NetworkAddress remote,
        std::shared_ptr<session_path_interface> _p,
        handlers::SessionEndpoint& parent,
        HopID remote_pivot_txid,
        session_tag _t,
        bool use_tun,
        shared_kx_data kx_data)
        : BaseSession{
            parent._router,
            std::move(_p),
            parent,
            std::move(remote),
            std::move(remote_pivot_txid),
            std::move(_t),
            use_tun,
            false,
            std::move(kx_data)}
    {
        log::debug(
            logcat,
            "InboundSession from remote client to local client {} created",
            _is_exit_session ? "exit" : "service");
    }

    std::shared_ptr<InboundClientSession> InboundClientSession::downcast(const std::shared_ptr<BaseSession>& b)
    {
        return std::dynamic_pointer_cast<InboundClientSession>(b);
    }

    void InboundClientSession::recv_path_switch(HopID remote_pivot_txid, std::shared_ptr<session_path_interface> new_pi)
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);
        set_remote_pivot_tx(std::move(remote_pivot_txid));
        set_new_current_path_interface(std::move(new_pi));
    }

    InboundRelaySession::InboundRelaySession(
        NetworkAddress remote,
        std::shared_ptr<session_path_interface> _p,
        handlers::SessionEndpoint& parent,
        HopID remote_pivot_txid,
        session_tag _t,
        bool use_tun,
        shared_kx_data kx_data)
        : InboundClientSession{
            std::move(remote),
            std::move(_p),
            parent,
            std::move(remote_pivot_txid),
            std::move(_t),
            use_tun,
            std::move(kx_data)}
    {
        log::debug(logcat, "InboundSession from remote client to local relay service created");
    }

    std::shared_ptr<InboundRelaySession> InboundRelaySession::downcast(const std::shared_ptr<BaseSession>& b)
    {
        return std::dynamic_pointer_cast<InboundRelaySession>(b);
    }

    bool InboundRelaySession::send_path_control_message(
        std::string method, std::string body, bt_control_response_hook func)
    {
        return _current_path->send_path_control_message(std::move(method), std::move(body), std::move(func));
    }

    bool InboundRelaySession::send_path_data_message(std::string data)
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);
        session_keys->encrypt(data);

        return _current_path->send_path_data_message(PATH::DATA::serialize_inner(std::move(data), _tag));
    }

}  // namespace llarp::session
