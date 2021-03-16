#pragma once

#include <llarp/service/endpoint.hpp>
#include "service/protocol_type.hpp"

namespace llarp
{
  namespace handlers
  {
    struct NullEndpoint final : public llarp::service::Endpoint,
                                public std::enable_shared_from_this<NullEndpoint>
    {
      NullEndpoint(AbstractRouter* r, llarp::service::Context* parent)
          : llarp::service::Endpoint(r, parent)
      {}

      virtual bool
      HandleInboundPacket(
          const service::ConvoTag, const llarp_buffer_t&, service::ProtocolType, uint64_t) override
      {
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
