#pragma once

#include <llarp/exit/endpoint.hpp>
#include "tun.hpp"
#include <llarp/dns/server.hpp>
#include <unordered_map>

namespace llarp
{
  struct AbstractRouter;
  namespace handlers
  {
    struct ExitEndpoint : public dns::Resolver_Base,
                          public EndpointBase,
                          public std::enable_shared_from_this<ExitEndpoint>
    {
      int
      Rank() const override
      {
        return 0;
      };

      std::string_view
      ResolverName() const override
      {
        return "snode";
      }

      bool
      MaybeHookDNS(
          std::shared_ptr<dns::PacketSource_Base> source,
          const dns::Message& query,
          const SockAddr& to,
          const SockAddr& from) override;

      ExitEndpoint(std::string name, AbstractRouter* r);
      ~ExitEndpoint() override;

      std::optional<AddressVariant_t>
      GetEndpointWithConvoTag(service::ConvoTag tag) const override;

      std::optional<service::ConvoTag>
      GetBestConvoTagFor(AddressVariant_t addr) const override;

      bool
      EnsurePathTo(
          AddressVariant_t addr,
          std::function<void(std::optional<service::ConvoTag>)> hook,
          llarp_time_t timeout) override;

      void
      LookupNameAsync(
          std::string name,
          std::function<void(std::optional<AddressVariant_t>)> resultHandler) override;

      const EventLoop_ptr&
      Loop() override;

      std::unordered_set<EndpointBase::AddressVariant_t>
      AllRemoteEndpoints() const override;

      void
      SRVRecordsChanged() override;

      void MarkAddressOutbound(AddressVariant_t) override{};

      bool
      SendToOrQueue(
          service::ConvoTag tag, const llarp_buffer_t& payload, service::ProtocolType t) override;

      void
      Tick(llarp_time_t now);

      void
      Configure(const NetworkConfig& networkConfig, const DnsConfig& dnsConfig);

      std::string
      Name() const;

      bool
      VisitEndpointsFor(const PubKey& pk, std::function<bool(exit::Endpoint* const)> visit) const;

      util::StatusObject
      ExtractStatus() const;

      bool
      SupportsV6() const;

      bool
      ShouldHookDNSMessage(const dns::Message& msg) const;

      bool
      HandleHookedDNSMessage(dns::Message msg, std::function<void(dns::Message)>);

      void
      LookupServiceAsync(
          std::string name,
          std::string service,
          std::function<void(std::vector<dns::SRVData>)> handler) override;

      bool
      AllocateNewExit(const PubKey pk, const PathID_t& path, bool permitInternet);

      exit::Endpoint*
      FindEndpointByPath(const PathID_t& path);

      exit::Endpoint*
      FindEndpointByIP(huint32_t ip);

      bool
      UpdateEndpointPath(const PubKey& remote, const PathID_t& next);

      /// handle ip packet from outside
      void
      OnInetPacket(net::IPPacket buf);

      AbstractRouter*
      GetRouter();

      llarp_time_t
      Now() const;

      template <typename Stats>
      void
      CalculateTrafficStats(Stats& stats)
      {
        for (auto& [pubkey, endpoint] : m_ActiveExits)
        {
          stats[pubkey].first += endpoint->TxRate();
          stats[pubkey].second += endpoint->RxRate();
        }
      }

      /// DO NOT CALL ME
      void
      DelEndpointInfo(const PathID_t& path);

      /// DO NOT CALL ME
      void
      RemoveExit(const exit::Endpoint* ep);

      bool
      QueueOutboundTraffic(net::IPPacket pkt);

      AddressVariant_t
      LocalAddress() const override;

      std::optional<SendStat>
      GetStatFor(AddressVariant_t remote) const override;

      /// sets up networking and starts traffic
      bool
      Start();

      bool
      Stop();

      bool
      ShouldRemove() const;

      bool
      HasLocalMappedAddrFor(const PubKey& pk) const;

      huint128_t
      GetIfAddr() const;

      void
      Flush();

      quic::TunnelManager*
      GetQUICTunnel() override;

      huint128_t
      GetIPForIdent(const PubKey pk);
      /// async obtain snode session and call callback when it's ready to send
      void
      ObtainSNodeSession(const RouterID& router, exit::SessionReadyFunc obtainCb);

     private:
      huint128_t
      AllocateNewAddress();

      /// obtain ip for service node session, creates a new session if one does
      /// not existing already
      huint128_t
      ObtainServiceNodeIP(const RouterID& router);

      bool
      QueueSNodePacket(const llarp_buffer_t& buf, huint128_t from);

      void
      MarkIPActive(huint128_t ip);

      void
      KickIdentOffExit(const PubKey& pk);

      AbstractRouter* m_Router;
      std::shared_ptr<dns::Server> m_Resolver;
      bool m_ShouldInitTun;
      std::string m_Name;
      bool m_PermitExit;
      std::unordered_map<PathID_t, PubKey> m_Paths;

      std::unordered_map<PubKey, exit::Endpoint*> m_ChosenExits;

      std::unordered_multimap<PubKey, std::unique_ptr<exit::Endpoint>> m_ActiveExits;

      using KeyMap_t = std::unordered_map<PubKey, huint128_t>;

      KeyMap_t m_KeyToIP;

      using SNodes_t = std::set<PubKey>;
      /// set of pubkeys we treat as snodes
      SNodes_t m_SNodeKeys;

      using SNodeSessions_t = std::unordered_map<RouterID, std::shared_ptr<exit::SNodeSession>>;
      /// snode sessions we are talking to directly
      SNodeSessions_t m_SNodeSessions;

      std::unordered_map<huint128_t, PubKey> m_IPToKey;

      huint128_t m_IfAddr;
      huint128_t m_HigestAddr;

      huint128_t m_NextAddr;
      IPRange m_OurRange;
      std::string m_ifname;

      std::unordered_map<huint128_t, llarp_time_t> m_IPActivity;

      std::shared_ptr<vpn::NetworkInterface> m_NetIf;

      SockAddr m_LocalResolverAddr;
      std::vector<SockAddr> m_UpstreamResolvers;

      std::shared_ptr<quic::TunnelManager> m_QUIC;

      using PacketQueue_t = std::
          priority_queue<net::IPPacket, std::vector<net::IPPacket>, net::IPPacket::CompareOrder>;

      /// internet to llarp packet queue
      PacketQueue_t m_InetToNetwork;
      bool m_UseV6;
      DnsConfig m_DNSConf;
    };
  }  // namespace handlers
}  // namespace llarp
