#pragma once

#include <llarp/net/ip_packet.hpp>
#include <llarp/util/buffer.hpp>
#include <llarp/util/logging.hpp>

#include <oxen/quic/address.hpp>
#include <oxen/quic/loop.hpp>
#include <oxen/quic/udp.hpp>

namespace llarp
{
    using UDPSocket = quic::UDPSocket;

    using io_result = quic::io_result;

    class UDPHandle
    {
      public:
        UDPHandle() = delete;
        explicit UDPHandle(const std::shared_ptr<quic::Loop>& ev, const quic::Address& bind, net_pkt_hook cb);
        ~UDPHandle();

      private:
        std::shared_ptr<quic::Loop> _loop;
        std::unique_ptr<UDPSocket> socket;
        quic::Address _local;

        void _send_or_queue(
            const quic::Path& path,
            std::vector<std::byte> buf,
            uint8_t ecn,
            std::function<void(io_result)> callback = nullptr);

      public:
        io_result send(const quic::Address& dest, std::span<const std::byte> data);

        quic::Address bind() { return _local; }
    };

}  //  namespace llarp
