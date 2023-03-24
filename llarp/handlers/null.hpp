#pragma once

#include <llarp/service/endpoint.hpp>
#include <llarp/service/protocol_type.hpp>
#include <llarp/quic/tunnel.hpp>
#include <llarp/router/abstractrouter.hpp>
#include <llarp/ev/ev.hpp>
#include <llarp/vpn/egres_packet_router.hpp>

namespace llarp::handlers
{
  struct NullEndpoint final : public llarp::service::Endpoint

  {
    NullEndpoint(AbstractRouter& r)
        : llarp::service::Endpoint{r}
        , m_PacketRouter{new vpn::EgresPacketRouter{[](auto from, auto pkt) {
          var::visit(
              [&pkt](auto&& from) {
                LogError("unhandled traffic from: ", from, " of ", pkt.size(), " bytes");
              },
              from);
        }}}
    {
      r.loop()->add_ticker([this] { Pump(Now()); });
    }

    bool
    HandleInboundPacket(
        const service::ConvoTag tag, std::vector<byte_t> buf, service::ProtocolType t, uint64_t)
    {
      LogTrace("Inbound ", t, " packet (", buf.size(), "B) on convo ", tag);
      if (t == service::ProtocolType::Control)
      {
        return true;
      }
      if (t == service::ProtocolType::TrafficV4 or t == service::ProtocolType::TrafficV6)
      {
        if (auto from = GetEndpointWithConvoTag(tag))
        {
          net::IPPacket pkt{std::move(buf)};
          if (pkt.empty())
          {
            LogWarn("invalid ip packet from remote T=", tag);
            return false;
          }
          m_PacketRouter->HandleIPPacketFrom(std::move(*from), std::move(pkt));
          return true;
        }
        else
        {
          LogWarn("did not handle packet, no endpoint with convotag T=", tag);
          return false;
        }
      }
      if (t != service::ProtocolType::QUIC)
        return false;

      auto* quic = GetQUICTunnel();
      if (!quic)
      {
        LogWarn("incoming quic packet but this endpoint is not quic capable; dropping");
        return false;
      }
      if (buf.size() < 4)
      {
        LogWarn("invalid incoming quic packet, dropping");
        return false;
      }
      quic->receive_packet(tag, std::move(buf));
      return true;
    }

    std::string
    GetIfName() const
    {
      return "";
    }

    bool
    SupportsV6() const
    {
      return false;
    }

    void
    SendPacketToRemote(const llarp_buffer_t&, service::ProtocolType) override{};

    vpn::EgresPacketRouter*
    EgresPacketRouter() override
    {
      return m_PacketRouter.get();
    }

   private:
    std::unique_ptr<vpn::EgresPacketRouter> m_PacketRouter;
  };
}  // namespace llarp::handlers
