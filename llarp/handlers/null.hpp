#ifndef LLARP_HANDLERS_NULL_HPP
#define LLARP_HANDLERS_NULL_HPP

#include <service/endpoint.hpp>

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
      SendPacketToRemote(const llarp_buffer_t&) override{};

      huint128_t
      ObtainIPForAddr(const AlignedBuffer<32>&, bool) override
      {
        return {0};
      }
    };
  }  // namespace handlers
}  // namespace llarp

#endif
