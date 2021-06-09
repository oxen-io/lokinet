#pragma once

#include "message.hpp"
#include <llarp/ev/ev.hpp>
#include <llarp/net/net.hpp>
#include "unbound_resolver.hpp"

#include <unordered_map>

namespace llarp
{
  namespace dns
  {
    /// handler of dns query hooking
    class IQueryHandler
    {
     public:
      virtual ~IQueryHandler() = default;

      /// return true if we should hook this message
      virtual bool
      ShouldHookDNSMessage(const Message& msg) const = 0;

      /// handle a hooked message
      virtual bool
      HandleHookedDNSMessage(Message query, std::function<void(Message)> sendReply) = 0;
    };

    // Base class for DNS lookups
    class PacketHandler : public std::enable_shared_from_this<PacketHandler>
    {
     public:
      explicit PacketHandler(EventLoop_ptr loop, IQueryHandler* handler);

      virtual ~PacketHandler() = default;

      virtual bool
      Start(
          SockAddr localaddr,
          std::vector<SockAddr> upstreamResolvers,
          std::vector<fs::path> hostfiles);

      void
      Stop();

      void
      Restart();

      void
      HandlePacket(const SockAddr& resolver, const SockAddr& from, llarp_buffer_t buf);

      bool
      ShouldHandlePacket(const SockAddr& to, const SockAddr& from, llarp_buffer_t buf) const;

     protected:
      virtual void
      SendServerMessageBufferTo(const SockAddr& from, const SockAddr& to, llarp_buffer_t buf) = 0;

     private:
      void
      HandleUpstreamFailure(const SockAddr& from, const SockAddr& to, Message msg);

      bool
      SetupUnboundResolver(std::vector<SockAddr> resolvers, std::vector<fs::path> hostfiles);

      IQueryHandler* const m_QueryHandler;
      std::set<SockAddr> m_Resolvers;
      std::shared_ptr<UnboundResolver> m_UnboundResolver;
      EventLoop_ptr m_Loop;
    };

    // Proxying DNS handler that listens on a UDP port for proper DNS requests.
    class Proxy : public PacketHandler
    {
     public:
      explicit Proxy(EventLoop_ptr loop, IQueryHandler* handler);

      bool
      Start(
          SockAddr localaddr,
          std::vector<SockAddr> upstreamResolvers,
          std::vector<fs::path> hostfiles) override;

     protected:
      void
      SendServerMessageBufferTo(
          const SockAddr& from, const SockAddr& to, llarp_buffer_t buf) override;

     private:
      std::shared_ptr<UDPHandle> m_Server;
      EventLoop_ptr m_Loop;
    };
  }  // namespace dns
}  // namespace llarp
