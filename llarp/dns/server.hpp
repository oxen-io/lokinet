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

    struct Proxy : public std::enable_shared_from_this<Proxy>
    {
      using Logic_ptr = std::shared_ptr<Logic>;
      Proxy(
          llarp_ev_loop_ptr serverLoop,
          Logic_ptr serverLogic,
          llarp_ev_loop_ptr clientLoop,
          Logic_ptr clientLogic,
          IQueryHandler* handler);

      bool
      Start(const IpAddress& addr, const std::vector<IpAddress>& resolvers);

      void
      Stop();

      void
      Restart();

      using Buffer_t = std::vector<uint8_t>;

     private:
      /// low level packet handler
      static void
      HandleUDPRecv_client(llarp_udp_io*, const SockAddr&, ManagedBuffer);
      static void
      HandleUDPRecv_server(llarp_udp_io*, const SockAddr&, ManagedBuffer);

      /// low level ticker
      static void
      HandleTick(llarp_udp_io*);

      void
      HandlePktClient(const SockAddr& from, Buffer_t buf);

      void
      HandlePktServer(const SockAddr& from, Buffer_t buf);

      void
      SendClientMessageTo(const SockAddr& to, Message msg);

      void
      SendServerMessageBufferTo(const SockAddr& to, const llarp_buffer_t& buf);

      void
      SendServerMessageTo(const SockAddr& to, Message msg);

      void
      HandleUpstreamResponse(SockAddr to, std::vector<byte_t> buf);

      void
      HandleUpstreamFailure(const SockAddr& to, Message msg);

      IpAddress
      PickRandomResolver() const;

      bool
      SetupUnboundResolver(const std::vector<IpAddress>& resolvers);

     private:
      llarp_udp_io m_Server;
      llarp_udp_io m_Client;
      llarp_ev_loop_ptr m_ServerLoop;
      llarp_ev_loop_ptr m_ClientLoop;
      Logic_ptr m_ServerLogic;
      Logic_ptr m_ClientLogic;
      IQueryHandler* m_QueryHandler;
      std::vector<IpAddress> m_Resolvers;
      std::shared_ptr<UnboundResolver> m_UnboundResolver;

      struct TX
      {
        MsgID_t txid;
        IpAddress from;

        bool
        operator==(const TX& other) const
        {
          return txid == other.txid && from == other.from;
        }

        struct Hash
        {
          size_t
          operator()(const TX& t) const noexcept
          {
            return t.txid ^ IpAddress::Hash()(t.from);
          }
        };
      };

      // maps tx to who to send reply to
      std::unordered_map<TX, IpAddress, TX::Hash> m_Forwarded;
    };
  }  // namespace dns
}  // namespace llarp

#endif
