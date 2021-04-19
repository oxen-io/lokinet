#pragma once
#include "policy.hpp"
#include <llarp/handlers/exit.hpp>

#include <string>
#include <unordered_map>

namespace llarp
{
  namespace exit
  {
    /// owner of all the exit endpoints
    struct Context
    {
      Context(AbstractRouter* r);
      ~Context();

      void
      Tick(llarp_time_t now);

      void
      ClearAllEndpoints();

      util::StatusObject
      ExtractStatus() const;

      /// send close to all exit sessions and remove all sessions
      void
      Stop();

      void
      AddExitEndpoint(
          const std::string& name, const NetworkConfig& networkConfig, const DnsConfig& dnsConfig);

      bool
      ObtainNewExit(const PubKey& remote, const PathID_t& path, bool permitInternet);

      exit::Endpoint*
      FindEndpointForPath(const PathID_t& path) const;

      /// calculate (pk, tx, rx) for all exit traffic
      using TrafficStats = std::unordered_map<PubKey, std::pair<uint64_t, uint64_t>>;

      void
      CalculateExitTraffic(TrafficStats& stats);

      std::shared_ptr<handlers::ExitEndpoint>
      GetExitEndpoint(std::string name) const;

     private:
      AbstractRouter* m_Router;
      std::unordered_map<std::string, std::shared_ptr<handlers::ExitEndpoint>> m_Exits;
      std::list<std::shared_ptr<handlers::ExitEndpoint>> m_Closed;
    };
  }  // namespace exit
}  // namespace llarp
