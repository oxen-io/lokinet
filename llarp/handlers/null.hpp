#pragma once

#include <llarp/ev/ev.hpp>
#include <llarp/link/tunnel.hpp>
#include <llarp/router/router.hpp>
#include <llarp/service/endpoint.hpp>
#include <llarp/service/protocol_type.hpp>
#include <llarp/vpn/egres_packet_router.hpp>

namespace llarp::handlers
{
  struct NullEndpoint final : public llarp::service::Endpoint,
                              public std::enable_shared_from_this<NullEndpoint>
  {
    NullEndpoint(Router* r, llarp::service::Context* parent)
        : llarp::service::Endpoint{r, parent}
        , m_PacketRouter{new vpn::EgresPacketRouter{[](auto from, auto pkt) {
          var::visit(
              [&pkt](auto&& from) {
                LogError("unhandled traffic from: ", from, " of ", pkt.size(), " bytes");
              },
              from);
        }}}
    {
      r->loop()->add_ticker([this] { Pump(Now()); });
    }

    bool
    HandleInboundPacket(
        const service::ConvoTag tag,
        const llarp_buffer_t& buf,
        service::ProtocolType t,
        uint64_t) override
    {
      LogTrace("Inbound ", t, " packet (", buf.sz, "B) on convo ", tag);
      if (t == service::ProtocolType::Control)
      {
        return true;
      }
      if (t == service::ProtocolType::TrafficV4 or t == service::ProtocolType::TrafficV6)
      {
        if (auto from = GetEndpointWithConvoTag(tag))
        {
          net::IPPacket pkt{};
          if (not pkt.Load(buf))
          {
            LogWarn("invalid ip packet from remote T=", tag);
            return false;
          }
          m_PacketRouter->HandleIPPacketFrom(std::move(*from), std::move(pkt));
          return true;
        }

        LogWarn("did not handle packet, no endpoint with convotag T=", tag);
        return false;
      }
      if (t != service::ProtocolType::QUIC)
        return false;

      auto* quic = GetQUICTunnel();
      if (!quic)
      {
        LogWarn("incoming quic packet but this endpoint is not quic capable; dropping");
        return false;
      }
      if (buf.sz < 4)
      {
        LogWarn("invalid incoming quic packet, dropping");
        return false;
      }
      // TODO:
      // quic->receive_packet(tag, buf);
      return true;
    }

    void send_packet_to_remote(std::string) override{};

    std::string
    GetIfName() const override
    {
      return "";
    }

    std::shared_ptr<path::PathSet>
    GetSelf() override
    {
      return shared_from_this();
    }

    std::weak_ptr<path::PathSet>
    GetWeak() override
    {
      return weak_from_this();
    }

    bool
    SupportsV6() const override
    {
      return false;
    }

    huint128_t
    ObtainIPForAddr(std::variant<service::Address, RouterID>) override
    {
      return {0};
    }

    std::optional<std::variant<service::Address, RouterID>>
    ObtainAddrForIP(huint128_t) const override
    {
      return std::nullopt;
    }

    vpn::EgresPacketRouter*
    EgresPacketRouter() override
    {
      return m_PacketRouter.get();
    }

   private:
    std::unique_ptr<vpn::EgresPacketRouter> m_PacketRouter;
  };
}  // namespace llarp::handlers
