#ifndef LLARP_HANDLERS_EXIT_HPP
#define LLARP_HANDLERS_EXIT_HPP

#include <exit/endpoint.hpp>
#include <handlers/tun.hpp>
#include <dns/server.hpp>
#include <unordered_map>

namespace llarp
{
  namespace handlers
  {
    struct ExitEndpoint : public llarp::dns::IQueryHandler
    {
      ExitEndpoint(const std::string& name, llarp::Router* r);
      ~ExitEndpoint();

      void
      Tick(llarp_time_t now);

      bool
      SetOption(const std::string& k, const std::string& v);

      std::string
      Name() const;

      bool
      ShouldHookDNSMessage(const llarp::dns::Message& msg) const override;

      bool
      HandleHookedDNSMessage(
          llarp::dns::Message,
          std::function< void(llarp::dns::Message) >) override;

      bool
      AllocateNewExit(const llarp::PubKey pk, const llarp::PathID_t& path,
                      bool permitInternet);

      llarp::exit::Endpoint*
      FindEndpointByPath(const llarp::PathID_t& path);

      llarp::exit::Endpoint*
      FindEndpointByIP(huint32_t ip);

      bool
      UpdateEndpointPath(const llarp::PubKey& remote,
                         const llarp::PathID_t& next);

      /// handle ip packet from outside
      void
      OnInetPacket(llarp_buffer_t buf);

      llarp::Router*
      Router();

      llarp_time_t
      Now() const;

      llarp::Crypto*
      Crypto();

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
      DelEndpointInfo(const llarp::PathID_t& path);

      /// DO NOT CALL ME
      void
      RemoveExit(const llarp::exit::Endpoint* ep);

      bool
      QueueOutboundTraffic(llarp_buffer_t buf);

      /// sets up networking and starts traffic
      bool
      Start();

      bool
      HasLocalMappedAddrFor(const llarp::PubKey& pk) const;

      huint32_t
      GetIfAddr() const;

      void
      Flush();

     private:
      huint32_t
      GetIPForIdent(const llarp::PubKey pk);

      huint32_t
      AllocateNewAddress();

      /// obtain ip for service node session, creates a new session if one does
      /// not existing already
      huint32_t
      ObtainServiceNodeIP(const llarp::RouterID& router);

      bool
      QueueSNodePacket(llarp_buffer_t buf, llarp::huint32_t from);

      void
      MarkIPActive(llarp::huint32_t ip);

      void
      KickIdentOffExit(const llarp::PubKey& pk);

      llarp::Router* m_Router;
      llarp::dns::Proxy m_Resolver;
      bool m_ShouldInitTun;
      std::string m_Name;
      bool m_PermitExit;
      std::unordered_map< llarp::PathID_t, llarp::PubKey,
                          llarp::PathID_t::Hash >
          m_Paths;

      std::unordered_map< llarp::PubKey, llarp::exit::Endpoint*,
                          llarp::PubKey::Hash >
          m_ChosenExits;

      std::unordered_multimap< llarp::PubKey,
                               std::unique_ptr< llarp::exit::Endpoint >,
                               llarp::PubKey::Hash >
          m_ActiveExits;

      using KeyMap_t = std::unordered_map< llarp::PubKey, llarp::huint32_t,
                                           llarp::PubKey::Hash >;

      KeyMap_t m_KeyToIP;

      using SNodes_t = std::set< llarp::PubKey >;
      /// set of pubkeys we treat as snodes
      SNodes_t m_SNodeKeys;

      using SNodeSessions_t =
          std::unordered_map< llarp::RouterID,
                              std::unique_ptr< llarp::exit::SNodeSession >,
                              llarp::RouterID::Hash >;
      /// snode sessions we are talking to directly
      SNodeSessions_t m_SNodeSessions;

      std::unordered_map< llarp::huint32_t, llarp::PubKey,
                          llarp::huint32_t::Hash >
          m_IPToKey;

      huint32_t m_IfAddr;
      huint32_t m_HigestAddr;
      huint32_t m_NextAddr;
      llarp::IPRange m_OurRange;

      std::unordered_map< llarp::huint32_t, llarp_time_t,
                          llarp::huint32_t::Hash >
          m_IPActivity;

      llarp_tun_io m_Tun;

      llarp::Addr m_LocalResolverAddr;
      std::vector< llarp::Addr > m_UpstreamResolvers;

      using Pkt_t = llarp::net::IPv4Packet;
      using PacketQueue_t =
          llarp::util::CoDelQueue< Pkt_t, Pkt_t::GetTime, Pkt_t::PutTime,
                                   Pkt_t::CompareOrder, Pkt_t::GetNow,
                                   llarp::util::DummyMutex,
                                   llarp::util::DummyLock, 5, 100, 1024 >;

      /// internet to llarp packet queue
      PacketQueue_t m_InetToNetwork;
    };
  }  // namespace handlers
}  // namespace llarp
#endif
