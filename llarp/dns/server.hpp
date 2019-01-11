#ifndef LLARP_DNS_SERVER_HPP
#define LLARP_DNS_SERVER_HPP

#include <dns/message.hpp>
#include <ev/ev.h>
#include <net/net.hpp>
#include <util/string_view.hpp>

#include <unordered_map>

namespace llarp
{
  namespace dns
  {
    /// handler of dns query hooking
    struct IQueryHandler
    {
      virtual ~IQueryHandler(){};

      /// return true if we should hook this message
      virtual bool
      ShouldHookDNSMessage(const Message& msg) const = 0;

      /// handle a hooked message
      virtual bool
      HandleHookedDNSMessage(dns::Message&& query,
                             std::function< void(Message) > sendReply) = 0;
    };

    struct Proxy
    {
      Proxy(llarp_ev_loop* loop, IQueryHandler* handler);

      bool
      Start(const llarp::Addr& addr,
            const std::vector< llarp::Addr >& resolvers);

      void
      Stop();

     private:
      /// low level packet handler
      static void
      HandleUDPRecv_client(llarp_udp_io*, const struct sockaddr*,
                           llarp_buffer_t);
      static void
      HandleUDPRecv_server(llarp_udp_io*, const struct sockaddr*,
                           llarp_buffer_t);

      /// low level ticker
      static void
      HandleTick(llarp_udp_io*);

      void
      Tick(llarp_time_t now);

      void
      HandlePktClient(llarp::Addr from, llarp_buffer_t* buf);

      void
      HandlePktServer(llarp::Addr from, llarp_buffer_t* buf);

      void
      SendMessageTo(llarp::Addr to, Message msg);

      llarp::Addr
      PickRandomResolver() const;

     private:
      llarp_udp_io m_Server;
      llarp_udp_io m_Client;
      llarp_ev_loop* m_Loop;
      IQueryHandler* m_QueryHandler;
      std::vector< llarp::Addr > m_Resolvers;

      struct TX
      {
        MsgID_t txid;
        llarp::Addr from;

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
            return t.txid ^ llarp::Addr::Hash()(t.from);
          }
        };
      };

      // maps tx to who to send reply to
      std::unordered_map< TX, llarp::Addr, TX::Hash > m_Forwarded;
    };
  }  // namespace dns
}  // namespace llarp

#endif
