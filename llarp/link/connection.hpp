#pragma once

#include <oxen/quic/btstream.hpp>
#include <oxen/quic/connection.hpp>
#include <oxen/quic/datagram.hpp>

namespace llarp
{
    namespace quic = oxen::quic;

    using quic::connection_closed_callback;
    using quic::connection_established_callback;
}  // namespace llarp

namespace llarp::link
{
    struct Connection
    {
        Connection(std::shared_ptr<quic::Connection> c, std::shared_ptr<quic::BTRequestStream> s);

        std::shared_ptr<quic::Connection> conn;
        std::shared_ptr<quic::Datagrams> datagrams;
        std::shared_ptr<quic::BTRequestStream> control_stream;

        bool is_inbound() const { return conn->is_inbound(); }

        void close_quietly();
    };
}  // namespace llarp::link
