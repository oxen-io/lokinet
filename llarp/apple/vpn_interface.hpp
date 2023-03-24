#pragma once

#include <llarp.hpp>
#include <llarp/vpn/platform.hpp>
#include <llarp/util/thread/queue.hpp>
#include <llarp/layers/platform/platform_layer.hpp>

namespace llarp::apple
{
  struct Context;

  using packet_write_callback = std::function<bool(int af_family, void* data, int size)>;

  class AppleVPNInterface final : public vpn::QueuedNetworkInterface,
                                  public std::enable_shared_from_this<AppleVPNInterface>
  {
   public:
    using on_readable_callback = std::function<void(AppleVPNInterface&)>;

    explicit AppleVPNInterface(
        Context& ctx,
        layers::platform::PlatformLayer& plat,
        packet_write_callback packet_writer,
        on_readable_callback on_readable);

    bool
    WritePacket(net::IPPacket pkt) override;

    /// continue reading packets.
    void
    on_readable();

   private:
    Context& _apple_ctx;
    // Function for us to call when we have a packet to emit.  Should return true if the packet was
    // handed off to the OS successfully.
    packet_write_callback _write_packet;
  };
  using on_readable_callback = AppleVPNInterface::on_readable_callback;
}  // namespace llarp::apple
