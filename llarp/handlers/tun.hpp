#pragma once

#include <llarp/dns/server.hpp>
#include <llarp/ev/ev.hpp>
#include <llarp/ev/vpn.hpp>
#include <llarp/net/ip.hpp>
#include <llarp/net/ip_packet.hpp>
#include <llarp/net/net.hpp>
#include <llarp/service/endpoint.hpp>
#include <llarp/util/codel.hpp>
#include <llarp/util/thread/threading.hpp>
#include <llarp/vpn/packet_router.hpp>

#include <future>
#include <queue>
#include <type_traits>
#include <variant>
#include "service/protocol_type.hpp"

namespace llarp
{
  namespace handlers
  {
    struct TunEndpoint : public service::Endpoint,
                         public dns::IQueryHandler,
                         public std::enable_shared_from_this<TunEndpoint>
    {
      TunEndpoint(AbstractRouter* r, llarp::service::Context* parent);
      ~TunEndpoint() override;

      path::PathSet_ptr
      GetSelf() override
      {
        return shared_from_this();
      }

      std::weak_ptr<path::PathSet>
      GetWeak() override
      {
        return weak_from_this();
      }

      void
      Thaw() override;

      bool
      Configure(const NetworkConfig& conf, const DnsConfig& dnsConf) override;

      void
      SendPacketToRemote(const llarp_buffer_t&, service::ProtocolType) override{};

      std::string
      GetIfName() const override;

      void
      Tick(llarp_time_t now) override;

      util::StatusObject
      ExtractStatus() const override;

      std::unordered_map<std::string, std::string>
      NotifyParams() const override;

      bool
      SupportsV6() const override;

      bool
      ShouldHookDNSMessage(const dns::Message& msg) const override;

      bool
      HandleHookedDNSMessage(
          dns::Message query, std::function<void(dns::Message)> sendreply) override;

      void
      TickTun(llarp_time_t now);

      bool
      MapAddress(const service::Address& remote, huint128_t ip, bool SNode);

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
      bool
      HandleInboundPacket(
          const service::ConvoTag tag,
          const llarp_buffer_t& pkt,
          service::ProtocolType t,
          uint64_t seqno) override;

      /// handle inbound traffic
      bool
      HandleWriteIPPacket(
          const llarp_buffer_t& buf, huint128_t src, huint128_t dst, uint64_t seqno);

      /// queue outbound packet to the world
      bool
      QueueOutboundTraffic(llarp::net::IPPacket&& pkt);

      /// we got a packet from the user
      void
      HandleGotUserPacket(llarp::net::IPPacket pkt);

      /// get the local interface's address
      huint128_t
      GetIfAddr() const override;

      /// we have an interface addr
      bool
      HasIfAddr() const override
      {
        return true;
      }

      bool
      HasLocalIP(const huint128_t& ip) const;

      std::optional<net::TrafficPolicy>
      GetExitPolicy() const override
      {
        return m_TrafficPolicy;
      }

      std::set<IPRange>
      GetOwnedRanges() const override
      {
        return m_OwnedRanges;
      }

      llarp_time_t
      PathAlignmentTimeout() const override
      {
        return m_PathAlignmentTimeout;
      }

      /// ip packet against any exit policies we have
      /// returns false if this traffic is disallowed by any of those policies
      /// returns true otherwise
      bool
      ShouldAllowTraffic(const net::IPPacket& pkt) const;

      /// get a key for ip address
      std::optional<std::variant<service::Address, RouterID>>
      ObtainAddrForIP(huint128_t ip) const override;

      bool
      HasAddress(const AlignedBuffer<32>& addr) const
      {
        return m_AddrToIP.find(addr) != m_AddrToIP.end();
      }

      /// get ip address for key unconditionally
      huint128_t
      ObtainIPForAddr(std::variant<service::Address, RouterID> addr) override;

      /// flush network traffic
      void
      Flush();

      void
      ResetInternalState() override;

     protected:
      using PacketQueue_t = llarp::util::CoDelQueue<
          net::IPPacket,
          net::IPPacket::GetTime,
          net::IPPacket::PutTime,
          net::IPPacket::CompareOrder,
          net::IPPacket::GetNow>;

      /// queue for sending packets over the network from us
      PacketQueue_t m_UserToNetworkPktQueue;

      struct WritePacket
      {
        uint64_t seqno;
        net::IPPacket pkt;

        bool
        operator<(const WritePacket& other) const
        {
          return other.seqno < seqno;
        }
      };

      /// queue for sending packets to user from network
      std::priority_queue<WritePacket> m_NetworkToUserPktQueue;
      /// return true if we have a remote loki address for this ip address
      bool
      HasRemoteForIP(huint128_t ipv4) const;

      /// mark this address as active
      void
      MarkIPActive(huint128_t ip);

      /// mark this address as active forever
      void
      MarkIPActiveForever(huint128_t ip);

      /// flush ip packets
      virtual void
      FlushSend();

      /// flush writing ip packets to interface
      void
      FlushWrite();

      /// maps ip to key (host byte order)
      std::unordered_map<huint128_t, AlignedBuffer<32>> m_IPToAddr;
      /// maps key to ip (host byte order)
      std::unordered_map<AlignedBuffer<32>, huint128_t> m_AddrToIP;

      /// maps key to true if key is a service node, maps key to false if key is
      /// a hidden service
      std::unordered_map<AlignedBuffer<32>, bool> m_SNodes;

     private:
      template <typename Addr_t, typename Endpoint_t>
      void
      SendDNSReply(
          Addr_t addr,
          Endpoint_t ctx,
          std::shared_ptr<dns::Message> query,
          std::function<void(dns::Message)> reply,
          bool sendIPv6)
      {
        if (ctx)
        {
          huint128_t ip = ObtainIPForAddr(addr);
          query->answers.clear();
          query->AddINReply(ip, sendIPv6);
        }
        else
          query->AddNXReply();
        reply(*query);
      }
      /// our dns resolver
      std::shared_ptr<dns::PacketHandler> m_Resolver;

      /// maps ip address to timestamp last active
      std::unordered_map<huint128_t, llarp_time_t> m_IPActivity;
      /// our ip address (host byte order)
      huint128_t m_OurIP;
      /// our network interface's ipv6 address
      huint128_t m_OurIPv6;

      /// next ip address to allocate (host byte order)
      huint128_t m_NextIP;
      /// highest ip address to allocate (host byte order)
      huint128_t m_MaxIP;
      /// our ip range we are using
      llarp::IPRange m_OurRange;
      /// upstream dns resolver list
      std::vector<SockAddr> m_UpstreamResolvers;
      /// dns host files list
      std::vector<fs::path> m_hostfiles;
      /// local dns
      IpAddress m_LocalResolverAddr;
      /// list of strict connect addresses for hooks
      std::vector<IpAddress> m_StrictConnectAddrs;
      /// use v6?
      bool m_UseV6;
      std::string m_IfName;

      std::optional<huint128_t> m_BaseV6Address;

      std::shared_ptr<vpn::NetworkInterface> m_NetIf;

      std::unique_ptr<vpn::PacketRouter> m_PacketRouter;

      std::optional<net::TrafficPolicy> m_TrafficPolicy;
      /// ranges we advetise as reachable
      std::set<IPRange> m_OwnedRanges;
      /// how long to wait for path alignment
      llarp_time_t m_PathAlignmentTimeout;
      /// idempotent wakeup for writing packets to user
      std::shared_ptr<EventLoopWakeup> m_PacketSendWaker;

      /// idempotent wakeup for writing messages to network
      std::shared_ptr<EventLoopWakeup> m_MessageSendWaker;

      /// a file to load / store the ephemeral address map to
      std::optional<fs::path> m_PersistAddrMapFile;
    };

  }  // namespace handlers
}  // namespace llarp
