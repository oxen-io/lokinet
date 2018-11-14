#ifndef LLARP_HANDLERS_EXIT_HPP
#define LLARP_HANDLERS_EXIT_HPP

#include <llarp/handlers/tun.hpp>
#include <llarp/exit/endpoint.hpp>
#include <unordered_map>

namespace llarp
{
  namespace handlers
  {
    struct ExitEndpoint final : public TunEndpoint
    {
      ExitEndpoint(const std::string& name, llarp_router* r);
      ~ExitEndpoint();

      void
      Tick(llarp_time_t now) override;

      bool
      SetOption(const std::string& k, const std::string& v) override;

      virtual std::string
      Name() const override;

      bool ShouldBuildMore(llarp_time_t) const
      {
        return false;
      }

      bool
      AllocateNewExit(const llarp::PubKey& pk, const llarp::PathID_t& path,
                      bool permitInternet);

      llarp::exit::Endpoint*
      FindEndpointByPath(const llarp::PathID_t& path);

      bool
      UpdateEndpointPath(const llarp::PubKey& remote,
                         const llarp::PathID_t& next);

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

      /// DO NOT CALL ME IF YOU DONT KNOW WHAT THIS DOES
      void
      DelEndpointInfo(const llarp::PathID_t& path, const huint32_t& ip,
                      const llarp::PubKey& pk);

      /// DO NOT CALL ME IF YOU DONT KNOW WHAT THIS DOES
      void
      RemoveExit(const llarp::exit::Endpoint* ep);

     protected:
      void
      FlushSend();

     private:
      std::string m_Name;
      bool m_PermitExit;
      std::unordered_map< llarp::PathID_t, llarp::PubKey,
                          llarp::PathID_t::Hash >
          m_Paths;
      std::unordered_multimap< llarp::PubKey, llarp::exit::Endpoint,
                               llarp::PubKey::Hash >
          m_ActiveExits;
    };
  }  // namespace handlers
}  // namespace llarp
#endif