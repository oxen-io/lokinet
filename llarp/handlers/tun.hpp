#ifndef LLARP_HANDLERS_TUN_HPP
#define LLARP_HANDLERS_TUN_HPP

#include <dns/server.hpp>
#include <ev/ev.h>
#include <net/ip.hpp>
#include <net/net.hpp>
#include <service/endpoint.hpp>
#include <util/codel.hpp>
#include <util/threading.hpp>

#include <future>

namespace llarp
{
  namespace handlers
  {
    static const int DefaultTunNetmask    = 16;
    static const char DefaultTunIfname[]  = "lokinet0";
    static const char DefaultTunDstAddr[] = "10.10.0.1";
    static const char DefaultTunSrcAddr[] = "10.10.0.2";

    struct TunEndpoint : public service::Endpoint,
                         public dns::IQueryHandler,
                         public std::enable_shared_from_this< TunEndpoint >
    {
      TunEndpoint(const std::string& nickname, AbstractRouter* r,
                  llarp::service::Context* parent);
      ~TunEndpoint();

      path::PathSet_ptr
      GetSelf() override
      {
        return shared_from_this();
      }

      virtual bool
      SetOption(const std::string& k, const std::string& v) override;

      virtual void
      Tick(llarp_time_t now) override;

      util::StatusObject
      ExtractStatus() const;

      std::unordered_map< std::string, std::string >
      NotifyParams() const override;

      bool
      ShouldHookDNSMessage(const dns::Message& msg) const override;

      bool
      HandleHookedDNSMessage(
          dns::Message&& query,
          std::function< void(dns::Message) > sendreply) override;

      void
      TickTun(llarp_time_t now);

      bool
      MapAddress(const service::Address& remote, huint32_t ip, bool SNode);

      bool
      Start() override;

      bool
      Stop() override;

      bool
      IsSNode() const;

      /// set up tun interface, blocking
      bool
      SetupTun();

      /// overrides Endpoint
      bool
      SetupNetworking() override;

      /// overrides Endpoint
      /// handle inbound traffic
      bool
      HandleWriteIPPacket(const llarp_buffer_t& buf,
                          std::function< huint32_t(void) > getFromIP) override;

      /// queue outbound packet to the world
      bool
      QueueOutboundTraffic(llarp::net::IPv4Packet&& pkt);

      /// we have a resolvable ip address
      bool
      HasIfAddr() const override
      {
        return true;
      }

      /// get the local interface's address
      huint32_t
      GetIfAddr() const override;

      bool
      HasLocalIP(const huint32_t& ip) const;

      llarp_tun_io tunif;
      std::unique_ptr< llarp_fd_promise > Promise;

      /// called before writing to tun interface
      static void
      tunifBeforeWrite(llarp_tun_io* t);

      /// handle user to network send buffer flush
      /// called in router logic thread
      static void
      handleNetSend(void*);

      /// called every time we wish to read a packet from the tun interface
      static void
      tunifRecvPkt(llarp_tun_io* t, const llarp_buffer_t& buf);

      /// called in the endpoint logic thread
      static void
      handleTickTun(void* u);

      /// get a key for ip address
      template < typename Addr >
      Addr
      ObtainAddrForIP(huint32_t ip, bool isSNode)
      {
        auto itr = m_IPToAddr.find(ip);
        if(itr == m_IPToAddr.end() || m_SNodes[itr->second] != isSNode)
        {
          // not found
          Addr addr;
          addr.Zero();
          return addr;
        }
        // found
        return Addr{itr->second};
      }

      bool
      HasAddress(const AlignedBuffer< 32 >& addr) const override
      {
        return m_AddrToIP.find(addr) != m_AddrToIP.end();
      }

      /// get ip address for key unconditionally
      huint32_t
      ObtainIPForAddr(const AlignedBuffer< 32 >& addr,
                      bool serviceNode) override;

      /// flush network traffic
      void
      Flush();

      virtual void
      ResetInternalState() override;

     protected:
      using PacketQueue_t = llarp::util::CoDelQueue<
          net::IPv4Packet, net::IPv4Packet::GetTime, net::IPv4Packet::PutTime,
          net::IPv4Packet::CompareOrder, net::IPv4Packet::GetNow >;
      /// queue for sending packets over the network from us
      PacketQueue_t m_UserToNetworkPktQueue;
      /// queue for sending packets to user from network
      PacketQueue_t m_NetworkToUserPktQueue;
      /// return true if we have a remote loki address for this ip address
      bool
      HasRemoteForIP(huint32_t ipv4) const;

      /// mark this address as active
      void
      MarkIPActive(huint32_t ip);

      /// mark this address as active forever
      void
      MarkIPActiveForever(huint32_t ip);

      /// flush ip packets
      virtual void
      FlushSend();

      /// maps ip to key (host byte order)
      std::unordered_map< huint32_t, AlignedBuffer< 32 >, huint32_t::Hash >
          m_IPToAddr;
      /// maps key to ip (host byte order)
      std::unordered_map< AlignedBuffer< 32 >, huint32_t,
                          AlignedBuffer< 32 >::Hash >
          m_AddrToIP;

      /// maps key to true if key is a service node, maps key to false if key is
      /// a hidden service
      std::unordered_map< AlignedBuffer< 32 >, bool, AlignedBuffer< 32 >::Hash >
          m_SNodes;

     private:
      bool
      QueueInboundPacketForExit(const llarp_buffer_t& buf)
      {
        ManagedBuffer copy{buf};
        return m_NetworkToUserPktQueue.EmplaceIf(
            [&](llarp::net::IPv4Packet& pkt) -> bool {
              if(!pkt.Load(copy.underlying))
                return false;
              pkt.UpdateIPv4PacketOnDst(pkt.src(), m_OurIP);
              return true;
            });
      }

      template < typename Addr_t, typename Endpoint_t >
      void
      SendDNSReply(Addr_t addr, Endpoint_t ctx, dns::Message* query,
                   std::function< void(dns::Message) > reply, bool snode,
                   bool sendIPv6)
      {
        if(ctx)
        {
          huint32_t ip = ObtainIPForAddr(addr, snode);
          query->AddINReply(ip, sendIPv6);
        }
        else
          query->AddNXReply();
        reply(*query);
        delete query;
      }

#ifndef WIN32
      /// handles fd injection force android
      std::promise< std::pair< int, int > > m_VPNPromise;
#endif

      /// our dns resolver
      std::shared_ptr<dns::Proxy> m_Resolver;

      /// maps ip address to timestamp last active
      std::unordered_map< huint32_t, llarp_time_t, huint32_t::Hash >
          m_IPActivity;
      /// our ip address (host byte order)
      huint32_t m_OurIP;
      /// next ip address to allocate (host byte order)
      huint32_t m_NextIP;
      /// highest ip address to allocate (host byte order)
      huint32_t m_MaxIP;
      /// our ip range we are using
      llarp::IPRange m_OurRange;
      /// upstream dns resolver list
      std::vector< llarp::Addr > m_UpstreamResolvers;
      /// local dns
      llarp::Addr m_LocalResolverAddr;
      /// list of strict connect addresses for hooks
      std::vector< llarp::Addr > m_StrictConnectAddrs;
    };
  }  // namespace handlers
}  // namespace llarp

#endif
