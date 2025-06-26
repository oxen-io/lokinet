#pragma once

#include <oxen/quic/btstream.hpp>
#include <oxen/quic/connection.hpp>
#include <oxen/quic/datagram.hpp>

namespace llarp
{
    namespace quic = oxen::quic;
}
namespace llarp::link
{
    struct Connection
    {
        Connection(
            std::shared_ptr<quic::Connection> c,
            std::shared_ptr<quic::BTRequestStream> s,
            bool _is_relay = true,
            bool _is_active = false);

        std::shared_ptr<quic::Connection> conn;
        std::shared_ptr<quic::Datagrams> datagrams;
        std::shared_ptr<quic::BTRequestStream> control_stream;

        std::atomic<bool> is_active{false};

        bool remote_is_relay{true};

        bool is_inbound() const { return conn->is_inbound(); }

        void close_quietly();
    };
}  // namespace llarp::link
