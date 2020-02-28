#pragma once 
#include "common.hpp"
#include "llarp.hpp"
#include "service/context.hpp"
#include "service/endpoint.hpp"
#include "router/abstractrouter.hpp"

namespace llarp 
{
namespace handlers
  {
    using Context_ptr = std::shared_ptr<llarp::Context>;

    struct PythonEndpoint final
        : public llarp::service::Endpoint,
          public std::enable_shared_from_this< PythonEndpoint >
    {

      PythonEndpoint(std::string name, Context_ptr routerContext)
          : llarp::service::Endpoint(name, routerContext->router.get(), &routerContext->router->hiddenServiceContext())
          , OurName(name)
      {
      }
      const std::string OurName;

      bool
      HandleInboundPacket(const service::ConvoTag tag, const llarp_buffer_t & pktbuf,
                          service::ProtocolType proto) override
      {
        if(handlePacket)
        {
          AlignedBuffer<32> addr;
          bool isSnode = false;
          if(not GetEndpointWithConvoTag(tag, addr, isSnode))
            return false;
          if(isSnode)
            return true;
          std::vector<byte_t> pkt;
          pkt.resize(pktbuf.sz);
          std::copy_n(pktbuf.base, pktbuf.sz, pkt.data());
          handlePacket(service::Address(addr), std::move(pkt), proto);
        }
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

      using PacketHandler_t = std::function<void(service::Address, std::vector<byte_t>, service::ProtocolType)>;

      PacketHandler_t handlePacket;

      void 
      SendPacket(service::Address remote, std::vector<byte_t> pkt, service::ProtocolType proto)
      {
        LogicCall(m_router->logic(), [remote, pkt, proto, self=shared_from_this()]() {
          self->SendToServiceOrQueue(remote, llarp_buffer_t(pkt), proto);
        });
      }

      std::string 
      GetOurAddress() const 
      {
        return m_Identity.pub.Addr().ToString();
      }
    };

    using PythonEndpoint_ptr = std::shared_ptr<PythonEndpoint>;
  }
}