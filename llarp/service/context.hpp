#ifndef LLARP_SERVICE_CONTEXT_HPP
#define LLARP_SERVICE_CONTEXT_HPP

#include <handlers/tun.hpp>
#include <net/net.hpp>
#include <config/config.hpp>
#include <service/endpoint.hpp>

#include <unordered_map>

namespace llarp
{
  namespace service
  {
    /// holds all the hidden service endpoints we own
    /// TODO: this should be refactored (removed entirely...?) now that lokinet
    ///       only supports one endpoint per instance
    struct Context
    {
      explicit Context(AbstractRouter* r);
      ~Context();

      void
      Tick(llarp_time_t now);

      /// stop all held services
      bool
      StopAll();

      util::StatusObject
      ExtractStatus() const;

      bool
      hasEndpoints();

      /// function visitor returns false to prematurely break iteration
      void
      ForEachService(std::function<bool(const std::string&, const Endpoint_ptr&)> visit) const;

      /// add endpoint via config
      void
      AddEndpoint(const NetworkConfig& conf, bool autostart = false);

      /// inject endpoint instance
      void
      InjectEndpoint(std::string name, std::shared_ptr<Endpoint> ep);

      /// stop and remove an endpoint by name
      /// return false if we don't have the hidden service with that name
      bool
      RemoveEndpoint(const std::string& name);

      Endpoint_ptr
      GetEndpointByName(const std::string& name);

      Endpoint_ptr
      GetDefault()
      {
        return GetEndpointByName("default");
      }

      bool
      StartAll();

     private:
      AbstractRouter* const m_Router;
      std::unordered_map<std::string, std::shared_ptr<Endpoint>> m_Endpoints;
      std::list<std::shared_ptr<Endpoint>> m_Stopped;
    };
  }  // namespace service
}  // namespace llarp
#endif
