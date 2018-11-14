#ifndef LLARP_HANDLERS_TUN_HPP
#define LLARP_HANDLERS_TUN_HPP
#include <llarp/ev.h>
#include <llarp/codel.hpp>
#include <llarp/ip.hpp>
#include <llarp/service/endpoint.hpp>
#include <llarp/threading.hpp>
#include <llarp/dnsd.hpp>               // for relay
#include <llarp/dns_dotlokilookup.hpp>  // for lookup
#include <llarp/dns_iptracker.hpp>      // for tracker

namespace llarp
{
  namespace handlers
  {
    static const int DefaultTunNetmask    = 16;
    static const char DefaultTunIfname[]  = "lokinet0";
    static const char DefaultTunDstAddr[] = "10.10.0.1";
    static const char DefaultTunSrcAddr[] = "10.10.0.2";

    struct TunEndpoint : public service::Endpoint
    {
      TunEndpoint(const std::string& nickname, llarp_router* r);
      ~TunEndpoint();

      virtual bool
      SetOption(const std::string& k, const std::string& v);

      virtual void
      Tick(llarp_time_t now);

      void
      TickTun(llarp_time_t now);

      bool
      MapAddress(const service::Address& remote, huint32_t ip);

      bool
      Start();

      /// set up tun interface, blocking
      bool
      SetupTun();

      /// overrides Endpoint
      bool
      SetupNetworking();

      /// overrides Endpoint
      /// handle inbound traffic
      bool
      ProcessDataMessage(service::ProtocolMessage* msg);

      /// queue outbound packet to the world
      bool
      QueueOutboundTraffic(llarp::net::IPv4Packet&& pkt);

      /// get the local interface's address
      huint32_t
      GetIfAddr() const;

      bool
      HasLocalIP(const huint32_t& ip) const;

#ifndef WIN32
      /// overrides Endpoint
      bool
      IsolationFailed()
      {
        m_TunSetupResult.set_value(false);
        return false;
      }
#endif

      llarp_tun_io tunif;

      /// called before writing to tun interface
      static void
      tunifBeforeWrite(llarp_tun_io* t);

      /// handle user to network send buffer flush
      /// called in router logic thread
      static void
      handleNetSend(void*);

      /// called every time we wish to read a packet from the tun interface
      static void
      tunifRecvPkt(llarp_tun_io* t, const void* pkt, ssize_t sz);

      /// called in the endpoint logic thread
      static void
      handleTickTun(void* u);

      /// get a key for ip address
      template < typename Addr >
      Addr
      ObtainAddrForIP(huint32_t ip)
      {
        auto itr = m_IPToAddr.find(ip);
        if(itr == m_IPToAddr.end())
        {
          // not found
          Addr addr;
          addr.Zero();
          return addr;
        }
        // found
        return itr->second.data();
      }

      bool
      HasAddress(const byte_t* addr) const
      {
        return m_AddrToIP.find(addr) != m_AddrToIP.end();
      }

      /// get ip address for key unconditionally
      huint32_t
      ObtainIPForAddr(const byte_t* addr);

     protected:
      typedef llarp::util::CoDelQueue<
          net::IPv4Packet, net::IPv4Packet::GetTime, net::IPv4Packet::PutTime,
          net::IPv4Packet::CompareOrder >
          PacketQueue_t;
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

     private:
      bool
      QueueInboundPacketForExit(llarp_buffer_t buf)
      {
        return m_NetworkToUserPktQueue.EmplaceIf(
            [&](llarp::net::IPv4Packet& pkt) -> bool {
              if(!pkt.Load(buf))
                return false;
              pkt.UpdateIPv4PacketOnDst(pkt.src(), m_OurIP);
              return true;
            });
      }

#ifndef WIN32
      /// handles setup, given value true on success and false on failure to set
      /// up interface
      std::promise< bool > m_TunSetupResult;
#endif
      /// DNS server per tun
      struct dnsd_context dnsd;
      /// DNS loki lookup subsystem configuration (also holds optional iptracker
      /// for netns)
      struct dotLokiLookup dll;

      /// maps ip address to timestamp last active
      std::unordered_map< huint32_t, llarp_time_t, huint32_t::Hash >
          m_IPActivity;
      /// our ip address (host byte order)
      huint32_t m_OurIP;
      /// next ip address to allocate (host byte order)
      huint32_t m_NextIP;
      /// highest ip address to allocate (host byte order)
      huint32_t m_MaxIP;
      /// upstream dns
      llarp::Addr m_UpstreamDNSAddr;
      /// local dns
      llarp::Addr m_LocalResolverAddr;
    };
  }  // namespace handlers
}  // namespace llarp

#endif
