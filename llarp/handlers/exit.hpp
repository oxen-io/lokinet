#ifndef LLARP_HANDLERS_EXIT_HPP
#define LLARP_HANDLERS_EXIT_HPP

#include <exit/endpoint.hpp>
#include <handlers/tun.hpp>
#include <dns/server.hpp>
#include <unordered_map>

namespace llarp
{
  struct AbstractRouter;
  namespace handlers
  {
    struct ExitEndpoint : public dns::IQueryHandler, public util::IStateful
    {
      ExitEndpoint(const std::string& name, AbstractRouter* r);
      ~ExitEndpoint();

      void
      Tick(llarp_time_t now);

      bool
      SetOption(const std::string& k, const std::string& v);

      std::string
      Name() const;

      util::StatusObject
      ExtractStatus() const override;

      bool
      ShouldHookDNSMessage(const dns::Message& msg) const override;

      bool
      HandleHookedDNSMessage(dns::Message&& msg,
                             std::function< void(dns::Message) >) override;

      bool
      AllocateNewExit(const PubKey pk, const PathID_t& path,
                      bool permitInternet);

      exit::Endpoint*
      FindEndpointByPath(const PathID_t& path);

      exit::Endpoint*
      FindEndpointByIP(huint32_t ip);

      bool
      UpdateEndpointPath(const PubKey& remote, const PathID_t& next);

      /// handle ip packet from outside
      void
      OnInetPacket(const llarp_buffer_t& buf);

      AbstractRouter*
      GetRouter();

      llarp_time_t
      Now() const;

      Crypto*
      GetCrypto();

      template < typename Stats >
      void
      CalculateTrafficStats(Stats& stats)
      {
        auto itr = m_ActiveExits.begin();
        while(itr != m_ActiveExits.end())
        {
          stats[itr->first].first += itr->second->TxRate();
          stats[itr->first].second += itr->second->RxRate();
          ++itr;
        }
      }

      /// DO NOT CALL ME
      void
      DelEndpointInfo(const PathID_t& path);

      /// DO NOT CALL ME
      void
      RemoveExit(const exit::Endpoint* ep);

      bool
      QueueOutboundTraffic(const llarp_buffer_t& buf);

      /// sets up networking and starts traffic
      bool
      Start();

      bool
      Stop();

      bool
      ShouldRemove() const;

      bool
      HasLocalMappedAddrFor(const PubKey& pk) const;

      huint32_t
      GetIfAddr() const;

      void
      Flush();

     private:
      huint32_t
      GetIPForIdent(const PubKey pk);

      huint32_t
      AllocateNewAddress();

      /// obtain ip for service node session, creates a new session if one does
      /// not existing already
      huint32_t
      ObtainServiceNodeIP(const RouterID& router);

      bool
      QueueSNodePacket(const llarp_buffer_t& buf, huint32_t from);

      void
      MarkIPActive(huint32_t ip);

      void
      KickIdentOffExit(const PubKey& pk);

      AbstractRouter* m_Router;
      dns::Proxy m_Resolver;
      bool m_ShouldInitTun;
      std::string m_Name;
      bool m_PermitExit;
      std::unordered_map< PathID_t, PubKey, PathID_t::Hash > m_Paths;

      std::unordered_map< PubKey, exit::Endpoint*, PubKey::Hash > m_ChosenExits;

      std::unordered_multimap< PubKey, std::unique_ptr< exit::Endpoint >,
                               PubKey::Hash >
          m_ActiveExits;

      using KeyMap_t = std::unordered_map< PubKey, huint32_t, PubKey::Hash >;

      KeyMap_t m_KeyToIP;

      using SNodes_t = std::set< PubKey >;
      /// set of pubkeys we treat as snodes
      SNodes_t m_SNodeKeys;

      using SNodeSessions_t =
          std::unordered_map< RouterID, std::unique_ptr< exit::SNodeSession >,
                              RouterID::Hash >;
      /// snode sessions we are talking to directly
      SNodeSessions_t m_SNodeSessions;

      std::unordered_map< huint32_t, PubKey, huint32_t::Hash > m_IPToKey;

      huint32_t m_IfAddr;
      huint32_t m_HigestAddr;
      huint32_t m_NextAddr;
      IPRange m_OurRange;

      std::unordered_map< huint32_t, llarp_time_t, huint32_t::Hash >
          m_IPActivity;

      llarp_tun_io m_Tun;

      Addr m_LocalResolverAddr;
      std::vector< Addr > m_UpstreamResolvers;

      using Pkt_t = net::IPv4Packet;
      using PacketQueue_t =
          util::CoDelQueue< Pkt_t, Pkt_t::GetTime, Pkt_t::PutTime,
                            Pkt_t::CompareOrder, Pkt_t::GetNow, util::NullMutex,
                            util::NullLock, 5, 100, 1024 >;

      /// internet to llarp packet queue
      PacketQueue_t m_InetToNetwork;
    };
  }  // namespace handlers
}  // namespace llarp
#endif
