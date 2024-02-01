#pragma once

#include <llarp.hpp>
#include <llarp/util/thread/queue.hpp>
#include <llarp/vpn/platform.hpp>

#include <memory>

namespace llarp::apple
{
    struct Context;

    class VPNInterface final : public vpn::NetworkInterface,
                               public std::enable_shared_from_this<VPNInterface>
    {
       public:
        using packet_write_callback = std::function<bool(int af_family, void* data, int size)>;
        using on_readable_callback = std::function<void(VPNInterface&)>;

        explicit VPNInterface(
            Context& ctx,
            packet_write_callback packet_writer,
            on_readable_callback on_readable,
            Router* router);

        // Method to call when a packet has arrived to deliver the packet to lokinet
        bool OfferReadPacket(const llarp_buffer_t& buf);

        int PollFD() const override;

        net::IPPacket ReadNextPacket() override;

        bool WritePacket(net::IPPacket pkt) override;

        void MaybeWakeUpperLayers() const override;

       private:
        // Function for us to call when we have a packet to emit.  Should return true if the packet
        // was handed off to the OS successfully.
        packet_write_callback m_PacketWriter;

        // Called when we are ready to start reading packets
        on_readable_callback m_OnReadable;

        static inline constexpr auto PacketQueueSize = 1024;

        thread::Queue<net::IPPacket> m_ReadQueue{PacketQueueSize};

        Router* const _router;
    };

}  // namespace llarp::apple
