#ifndef LLARP_HANDLERS_TUN_HPP
#define LLARP_HANDLERS_TUN_HPP
#include <llarp/ev.h>
#include <llarp/codel.hpp>
#include <llarp/ip.hpp>
#include <llarp/service/endpoint.hpp>
#include <llarp/threading.hpp>

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

      bool
      SetOption(const std::string& k, const std::string& v);

      void
      Tick(llarp_time_t now);

      void
      TickTun(llarp_time_t now);

      bool
      Start();

      /// set up tun interface, blocking
      bool
      SetupTun();

      /// overrides Endpoint
      bool
      SetupNetworking();

      llarp_tun_io tunif;

      static void
      tunifBeforeWrite(llarp_tun_io* t);

      static void
      tunifRecvPkt(llarp_tun_io* t, const void* pkt, ssize_t sz);

      static void
      handleTickTun(void* u);

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
      HasRemoteForIP(const uint32_t& ipv4)
      {
        return m_IPs.find(ipv4) != m_IPs.end();
      }

     private:
      std::promise< bool > m_TunSetupResult;
      std::unordered_map< uint32_t, service::Address > m_IPs;
    };
  }  // namespace handlers
}  // namespace llarp

#endif
