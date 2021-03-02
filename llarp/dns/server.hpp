#ifndef LLARP_DNS_SERVER_HPP
#define LLARP_DNS_SERVER_HPP

#include <dns/message.hpp>
#include <ev/ev.h>
#include <net/net.hpp>
#include <util/thread/logic.hpp>
#include <dns/unbound_resolver.hpp>

#include <unordered_map>

namespace llarp
{
  namespace dns
  {
    /// handler of dns query hooking
    struct IQueryHandler
    {
      virtual ~IQueryHandler() = default;

      /// return true if we should hook this message
      virtual bool
      ShouldHookDNSMessage(const Message& msg) const = 0;

      /// handle a hooked message
      virtual bool
      HandleHookedDNSMessage(Message query, std::function<void(Message)> sendReply) = 0;
    };

    struct PacketHandler : public std::enable_shared_from_this<PacketHandler>
    {
      using Logic_ptr = std::shared_ptr<Logic>;
      using Buffer_t = std::vector<uint8_t>;

      explicit PacketHandler(Logic_ptr logic, IQueryHandler* handler);

      virtual ~PacketHandler() = default;

      virtual bool
      Start(SockAddr localaddr, std::vector<IpAddress> upstreamResolvers);

      void
      Stop();

      void
      Restart();

      void
      HandlePacket(SockAddr resolver, SockAddr from, Buffer_t buf);

      bool
      ShouldHandlePacket(SockAddr to, SockAddr from, Buffer_t buf) const;

     protected:
      virtual void
      SendServerMessageBufferTo(SockAddr from, SockAddr to, Buffer_t buf) = 0;

     private:
      void
      HandleUpstreamFailure(SockAddr from, SockAddr to, Message msg);

      bool
      SetupUnboundResolver(std::vector<IpAddress> resolvers);

      IQueryHandler* const m_QueryHandler;
      std::set<IpAddress> m_Resolvers;
      std::shared_ptr<UnboundResolver> m_UnboundResolver;
      Logic_ptr m_Logic;
    };

    struct Proxy : public PacketHandler
    {
      using Logic_ptr = std::shared_ptr<Logic>;
      explicit Proxy(llarp_ev_loop_ptr loop, Logic_ptr logic, IQueryHandler* handler);

      bool
      Start(SockAddr localaddr, std::vector<IpAddress> resolvers) override;

      using Buffer_t = std::vector<uint8_t>;

     protected:
      void
      SendServerMessageBufferTo(SockAddr from, SockAddr to, Buffer_t buf) override;

     private:
      static void
      HandleUDP(llarp_udp_io*, const SockAddr&, ManagedBuffer);

     private:
      llarp_udp_io m_Server;
      llarp_ev_loop_ptr m_Loop;
    };
  }  // namespace dns
}  // namespace llarp

#endif
