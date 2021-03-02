#ifndef LLARP_DNS_SERVER_HPP
#define LLARP_DNS_SERVER_HPP

#include <dns/message.hpp>
#include <ev/ev.hpp>
#include <net/net.hpp>
#include <util/thread/logic.hpp>
#include <dns/unbound_resolver.hpp>

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
      using Logic_ptr = std::shared_ptr<Logic>;

      explicit PacketHandler(Logic_ptr logic, IQueryHandler* handler);

      virtual ~PacketHandler() = default;

      virtual bool
      Start(SockAddr localaddr, std::vector<IpAddress> upstreamResolvers);

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
      SetupUnboundResolver(std::vector<IpAddress> resolvers);

      IQueryHandler* const m_QueryHandler;
      std::set<IpAddress> m_Resolvers;
      std::shared_ptr<UnboundResolver> m_UnboundResolver;
      Logic_ptr m_Logic;
    };

    // Proxying DNS handler that listens on a UDP port for proper DNS requests.
    class Proxy : public PacketHandler
    {
     public:
      using Logic_ptr = std::shared_ptr<Logic>;
      explicit Proxy(EventLoop_ptr loop, Logic_ptr logic, IQueryHandler* handler);

      bool
      Start(SockAddr localaddr, std::vector<IpAddress> resolvers) override;

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

#endif
