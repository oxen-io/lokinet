#ifndef LLARP_HANDLERS_EXIT_HPP
#define LLARP_HANDLERS_EXIT_HPP

#include <llarp/handlers/tun.hpp>
#include <llarp/exit/endpoint.hpp>
#include <unordered_map>

namespace llarp
{
  namespace handlers
  {
    struct ExitEndpoint
    {
      ExitEndpoint(const std::string& name, llarp_router* r);
      ~ExitEndpoint();

      void
      Tick(llarp_time_t now);

      bool
      SetOption(const std::string& k, const std::string& v);

      virtual std::string
      Name() const;

      bool
      AllocateNewExit(const llarp::PubKey& pk, const llarp::PathID_t& path,
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

      llarp_router*
      Router();

      llarp_crypto*
      Crypto();

      template < typename Stats >
      void
      CalculateTrafficStats(Stats& stats)
      {
        auto itr = m_ActiveExits.begin();
        while(itr != m_ActiveExits.end())
        {
          stats[itr->first].first += itr->second.TxRate();
          stats[itr->first].second += itr->second.RxRate();
          ++itr;
        }
      }

      /// DO NOT CALL ME
      void
      DelEndpointInfo(const llarp::PathID_t& path, const huint32_t& ip,
                      const llarp::PubKey& pk);

      /// DO NOT CALL ME
      void
      RemoveExit(const llarp::exit::Endpoint* ep);

      bool
      QueueOutboundTraffic(llarp_buffer_t buf);

      /// sets up networking and starts traffic
      bool
      Start();

      huint32_t
      GetIfAddr() const;

      void
      FlushInbound();

     private:
      huint32_t
      GetIPForIdent(const llarp::PubKey pk);

      huint32_t
      AllocateNewAddress();

      void
      MarkIPActive(llarp::huint32_t ip);

      void
      KickIdentOffExit(const llarp::PubKey& pk);

      llarp_router* m_Router;
      std::string m_Name;
      bool m_PermitExit;
      std::unordered_map< llarp::PathID_t, llarp::PubKey,
                          llarp::PathID_t::Hash >
          m_Paths;
      std::unordered_multimap< llarp::PubKey, llarp::exit::Endpoint,
                               llarp::PubKey::Hash >
          m_ActiveExits;

      std::unordered_map< llarp::PubKey, llarp::huint32_t, llarp::PubKey::Hash >
          m_KeyToIP;

      std::unordered_map< llarp::huint32_t, llarp::PubKey,
                          llarp::huint32_t::Hash >
          m_IPToKey;

      huint32_t m_IfAddr;
      huint32_t m_HigestAddr;
      huint32_t m_NextAddr;

      std::unordered_map< llarp::huint32_t, llarp_time_t,
                          llarp::huint32_t::Hash >
          m_IPActivity;

      llarp_tun_io m_Tun;

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
