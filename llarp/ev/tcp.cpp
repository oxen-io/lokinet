#include "tcp.hpp"

#include <llarp/net/ip_packet.hpp>
#include <llarp/util/logging/buffer.hpp>

namespace llarp
{
    static auto logcat = oxen::log::Cat("ev-tcp");

    constexpr auto evconnlistener_deleter = [](::evconnlistener *e) {
        log::trace(logcat, "Invoking evconnlistener deleter!");
        if (e)
            evconnlistener_free(e);
    };

    /// Checks rv for being -1 and, if so, raises a system_error from errno.  Otherwise returns it.
    static int check_rv(int rv)
    {
#ifdef _WIN32
        if (rv == SOCKET_ERROR)
            throw std::system_error{WSAGetLastError(), std::system_category()};
#else
        if (rv == -1)
            throw std::system_error{errno, std::system_category()};
#endif
        return rv;
    }

    static void tcp_read_cb(bufferevent *bev, void *user_arg)
    {
        std::vector<uint8_t> buf{};
        buf.resize(2048);

        // Load data from input buffer to local buffer
        // FIXME: handle nwrite == 0
        auto nwrite = bufferevent_read(bev, buf.data(), buf.size());
        buf.resize(nwrite);

        log::trace(logcat, "TCP socket received {}B: {}", nwrite, buffer_printer{buf});

        auto *conn = reinterpret_cast<TCPConnection *>(user_arg);
        assert(conn);

        conn->stream->send(std::move(buf));
    };

    static void tcp_write_cb([[maybe_unused]] bufferevent *bev, void *user_arg)
    {
        auto *conn = reinterpret_cast<TCPConnection *>(user_arg);
        conn->on_write_available();
    }

    void TCPConnection::on_write_available()
    {
        log::debug(logcat, "TCP Tunnel connection, write to local conn was blocked but is now available.");
        stream->resume();
    }

    static void tcp_event_cb(bufferevent *bev, short what, void *user_arg)
    {
        (void)bev;
        (void)user_arg;
        auto *conn = reinterpret_cast<TCPConnection *>(user_arg);
        assert(conn);

        log::log(
            logcat,
            what & BEV_EVENT_ERROR ? log::Level::err : log::Level::debug,
            "TCP Connection {} event: {}",
            what & BEV_EVENT_READING ? "READ" : "WRITE",
            what & BEV_EVENT_EOF             ? "EOF"
                : what & BEV_EVENT_ERROR     ? "ERROR"
                : what & BEV_EVENT_TIMEOUT   ? "TIMEOUT"
                : what & BEV_EVENT_CONNECTED ? "CONNECTED"
                                             : "IMPOSSIBLE");

        // this is where the InboundSession confirms it established a TCP connection to the backend app
        if (what & BEV_EVENT_CONNECTED)
        {
            log::info(logcat, "TCP connect operation finished!");
            conn->stream->resume();
        }
        if (what & BEV_EVENT_ERROR)
        {
            log::critical(logcat, "TCP Connection encountered error from bufferevent");
        }
        if (what & (BEV_EVENT_EOF | BEV_EVENT_ERROR))
        {
            log::debug(logcat, "TCP Connection closing tunneled QUIC stream");

            conn->stream->close();
        }
    };

    static void tcp_listen_cb(
        struct evconnlistener *listener, evutil_socket_t fd, struct sockaddr *src, int socklen, void *user_arg)
    {
        quic::Address source{src, static_cast<socklen_t>(socklen)};
        log::debug(logcat, "TCP RECEIVED -- SRC:{}", source);

        auto *b = evconnlistener_get_base(listener);
        auto *bevent = bufferevent_socket_new(b, fd, BEV_OPT_CLOSE_ON_FREE | BEV_OPT_THREADSAFE);

        auto *handle = reinterpret_cast<TCPHandle *>(user_arg);
        assert(handle);

        // make TCPConnection here!
        auto *conn = handle->_conn_maker(bevent, fd);

        bufferevent_setcb(bevent, tcp_read_cb, tcp_write_cb, tcp_event_cb, conn);
        bufferevent_enable(bevent, EV_READ | EV_WRITE);
    };

    static void tcp_err_cb(struct evconnlistener * /* e */, void *user_arg)
    {
        int ec = EVUTIL_SOCKET_ERROR();
        log::critical(logcat, "TCP LISTENER RECEIVED ERROR CODE {}:{}", ec, evutil_socket_error_to_string(ec));

        auto *handle = reinterpret_cast<TCPHandle *>(user_arg);
        assert(handle);
        (void)handle;

        // DISCUSS: close everything here?
    };

    TCPConnection::TCPConnection(bufferevent *_bev, evutil_socket_t _fd, std::shared_ptr<quic::Stream> s)
        : bev{_bev}, fd{_fd}, stream{std::move(s)}
    {
        stream->set_stream_data_cb([this, _bev](quic::Stream &stream, std::span<const std::byte> data) mutable {
            // libquic FIXME: would be convenient to be able to ask the stream if it's inbound or outbound
            // here since this callback is used for both and that would make logging more clear.
            if (stream.is_paused())
            {
                // FIXME: C++23 makes this syntax nicer
                pending_buffer.insert(pending_buffer.end(), data.begin(), data.end());
                return;
            }
            std::byte *cur = pending_buffer.data();
            size_t written = 0;
            while (written < pending_buffer.size())
            {
                constexpr size_t chunk_size = 1500;  // FIXME: this, obviously; ass number
                size_t s = std::min(chunk_size, pending_buffer.size() - written);
                if (bufferevent_write(_bev, cur, s) != 0)
                {
                    // FIXME: if hypothetically quic/lokinet stream is finished sending,
                    // so this is the last call of this callback, but socket is blocked, not
                    // letting us write the last chunk(s) buffered, what to do?
                    break;
                }
                written += s;
                cur += s;
            }
            if (written < pending_buffer.size())
            {
                log::debug(logcat, "TCP Tunnel stream unpaused, but we failed to write all queued data.");
                size_t new_size = pending_buffer.size() - written;
                std::memmove(pending_buffer.data(), pending_buffer.data() + written, new_size);
                pending_buffer.resize(new_size);
                pending_buffer.insert(pending_buffer.end(), data.begin(), data.end());
            }
            if (written == pending_buffer.size())
                pending_buffer.clear();
            else
            {  // we got unpaused, but clogged the (kernel?) buffer again, pause again
                stream.pause();
                pending_buffer.insert(pending_buffer.end(), data.begin(), data.end());
                return;
            }

            if (auto rv = bufferevent_write(_bev, data.data(), data.size()); rv != 0)
            {
                log::debug(logcat, "TCP Tunnel refused write, pausing quic stream and buffering.");
                pending_buffer.resize(data.size());
                std::memcpy(pending_buffer.data(), data.data(), data.size());
                return;
            }

            log::debug(logcat, "Stream (id:{}) wrote {}B to TCP buffer", stream.stream_id(), data.size());
        });
    }

    void TCPConnection::stop_reading() { bufferevent_disable(bev, EV_READ); }

    void TCPConnection::resume_reading() { bufferevent_enable(bev, EV_READ); }

    TCPConnection::~TCPConnection()
    {
        bufferevent_free(bev);
        log::debug(logcat, "TCPSocket shut down!");
    }

    void TCPConnection::close(uint64_t ec)
    {
        log::info(logcat, "TCP connection closing with application error code: {}", ec);
    }

    std::shared_ptr<TCPHandle> TCPHandle::make_server(
        const std::shared_ptr<quic::Loop> &ev, tcpconn_hook cb, uint16_t port)
    {
        std::shared_ptr<TCPHandle> h{new TCPHandle(ev, std::move(cb), port)};
        return h;
    }

    std::shared_ptr<TCPHandle> TCPHandle::make_client(const std::shared_ptr<quic::Loop> &ev, quic::Address connect)
    {
        std::shared_ptr<TCPHandle> h{new TCPHandle{ev, std::move(connect)}};
        return h;
    }

    TCPHandle::TCPHandle(const std::shared_ptr<quic::Loop> &ev_loop, quic::Address connect)
        : _ev{ev_loop}, _connect{std::move(connect)}
    {
        assert(_ev);
    }

    TCPHandle::TCPHandle(const std::shared_ptr<quic::Loop> &ev_loop, tcpconn_hook cb, uint16_t p)
        : _ev{ev_loop}, _conn_maker{std::move(cb)}
    {
        assert(_ev);

        if (!_conn_maker)
            throw std::logic_error{"TCPSocket construction requires a non-empty receive callback"};

        _init_server(p);
    }

    std::shared_ptr<TCPConnection> TCPHandle::connect(
        event_base *_ev, quic::Address src, std::shared_ptr<quic::Stream> s, uint16_t port)
    {
        sockaddr_in _addr = src.in4();
        _addr.sin_port = htonl(port);

        // NB: BEV_OPT_THREADSAFE not used because this should only ever be touched
        // by a single thread.
        bufferevent *_bev = bufferevent_socket_new(_ev, -1, BEV_OPT_CLOSE_ON_FREE);

        if (bufferevent_socket_connect(_bev, (struct sockaddr *)&_addr, sizeof(_addr)) < 0)
        {
            log::warning(logcat, "Failed to make bufferevent-based TCP connection!");
            return nullptr;
        }

        auto tcp_conn = std::make_shared<TCPConnection>(_bev, -1, std::move(s));

        // only set after a call to bufferevent_socket_connect
        tcp_conn->fd = bufferevent_getfd(_bev);

        return tcp_conn;
    }

    void TCPHandle::_init_client() {}

    void TCPHandle::_init_server(uint16_t port)
    {
        sockaddr_in _tcp{};
        _tcp.sin_family = AF_INET;
        _tcp.sin_addr.s_addr = INADDR_ANY;
        _tcp.sin_port = htonl(port);

        _tcp_listener = _ev->template shared_ptr<struct evconnlistener>(
            evconnlistener_new_bind(
                _ev->get_event_base(),
                tcp_listen_cb,
                this,
                LEV_OPT_CLOSE_ON_FREE | LEV_OPT_THREADSAFE | LEV_OPT_REUSEABLE,
                -1,
                reinterpret_cast<sockaddr *>(&_tcp),
                sizeof(sockaddr)),
            evconnlistener_deleter);

        if (not _tcp_listener)
        {
            throw std::runtime_error{
                "TCP listener construction failed: {}"_format(evutil_socket_error_to_string(EVUTIL_SOCKET_ERROR()))};
        }

        _sock = evconnlistener_get_fd(_tcp_listener.get());
        check_rv(getsockname(_sock, _bound, _bound.socklen_ptr()));
        log::debug(logcat, "tcp listener, bound to {}", _bound);
        evconnlistener_set_error_cb(_tcp_listener.get(), tcp_err_cb);
    }

    TCPHandle::~TCPHandle()
    {
        _tcp_listener.reset();
        log::debug(logcat, "TCPHandle shut down!");
    }
}  //  namespace llarp
