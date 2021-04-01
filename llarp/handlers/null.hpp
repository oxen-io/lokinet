#pragma once

#include <llarp/service/endpoint.hpp>
#include <llarp/service/protocol_type.hpp>
#include <llarp/quic/tunnel.hpp>
#include <llarp/router/abstractrouter.hpp>
#include <llarp/ev/ev.hpp>

namespace llarp
{
  namespace handlers
  {
    struct NullEndpoint final : public llarp::service::Endpoint,
                                public std::enable_shared_from_this<NullEndpoint>
    {
      NullEndpoint(AbstractRouter* r, llarp::service::Context* parent)
          : llarp::service::Endpoint(r, parent)
      {
        r->loop()->add_ticker([this] {
          while (not m_InboundQuic.empty())
          {
            m_InboundQuic.top().process();
            m_InboundQuic.pop();
          }
          Pump(Now());
        });
      }

      struct QUICEvent
      {
        uint64_t seqno;
        std::function<void()> process;

        bool
        operator<(const QUICEvent& other) const
        {
          return other.seqno < seqno;
        }
      };

      std::priority_queue<QUICEvent> m_InboundQuic;

      virtual bool
      HandleInboundPacket(
          const service::ConvoTag tag,
          const llarp_buffer_t& buf,
          service::ProtocolType t,
          uint64_t seqno) override
      {
        LogTrace("Inbound ", t, " packet (", buf.sz, "B) on convo ", tag);
        if (t == service::ProtocolType::Control)
        {
          MarkConvoTagActive(tag);
          return true;
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
        MarkConvoTagActive(tag);
        std::vector<byte_t> copy;
        copy.resize(buf.sz);
        std::copy_n(buf.base, buf.sz, copy.data());
        m_InboundQuic.push({seqno, [quic, buf = copy, tag]() { quic->receive_packet(tag, buf); }});
        m_router->loop()->wakeup();
        return true;
      }

      std::string
      GetIfName() const override
      {
        return "";
      }

      path::PathSet_ptr
      GetSelf() override
      {
        return shared_from_this();
      }

      bool
      SupportsV6() const override
      {
        return false;
      }

      void
      SendPacketToRemote(const llarp_buffer_t&, service::ProtocolType) override{};

      huint128_t ObtainIPForAddr(std::variant<service::Address, RouterID>) override
      {
        return {0};
      }

      std::optional<std::variant<service::Address, RouterID>> ObtainAddrForIP(
          huint128_t) const override
      {
        return std::nullopt;
      }
    };
  }  // namespace handlers
}  // namespace llarp
