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
      NullEndpoint(const SnappConfig& conf, AbstractRouter* r, llarp::service::Context* parent)
          : llarp::service::Endpoint(conf, r, parent)
      {
      }

      virtual bool
      HandleInboundPacket(
          const service::ConvoTag, const llarp_buffer_t&, service::ProtocolType) override
      {
        return true;
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
    };
  }  // namespace handlers
}  // namespace llarp

#endif
