#include <llarp/handlers/tun.hpp>
#include <llarp/service/context.hpp>

namespace llarp
{
  namespace service
  {
    Context::Context(llarp_router *r) : m_Router(r)
    {
    }

    Context::~Context()
    {
      auto itr = m_Endpoints.begin();
      while(itr != m_Endpoints.end())
      {
        itr = m_Endpoints.erase(itr);
      }
    }

    void
    Context::Tick()
    {
      auto now = llarp_time_now_ms();
      auto itr = m_Endpoints.begin();
      while(itr != m_Endpoints.end())
      {
        itr->second->Tick(now);
        ++itr;
      }
    }

    bool
    Context::AddEndpoint(const Config::section_t &conf)
    {
      {
        auto itr = m_Endpoints.find(conf.first);
        if(itr != m_Endpoints.end())
        {
          llarp::LogError("cannot add hidden service with duplicate name: ",
                          conf.first);
          return false;
        }
      }
      // extract type
      std::string endpointType = "tun";
      for(const auto &option : conf.second)
      {
        if(option.first == "type")
          endpointType = option.second;
      }
      std::unique_ptr< llarp::service::Endpoint > service;

      static std::map< std::string,
                       std::function< llarp::service::Endpoint *(
                           const std::string &, llarp_router *) > >
          endpointConstructors = {
              {"tun",
               [](const std::string &nick,
                  llarp_router *r) -> llarp::service::Endpoint * {
                 return new llarp::handlers::TunEndpoint(nick, r);
               }},
              {"null",
               [](const std::string &nick,
                  llarp_router *r) -> llarp::service::Endpoint * {
                 return new llarp::service::Endpoint(nick, r);
               }}};

      {
        // detect type
        auto itr = endpointConstructors.find(endpointType);
        if(itr == endpointConstructors.end())
        {
          llarp::LogError("no such endpoint type: ", endpointType);
          return false;
        }

        // construct
        service = std::unique_ptr< llarp::service::Endpoint >(
            itr->second(conf.first, m_Router));
      }
      // configure
      for(const auto &option : conf.second)
      {
        auto &k = option.first;
        if(k == "type")
          continue;
        auto &v = option.second;
        if(!service->SetOption(k, v))
        {
          llarp::LogError("failed to set ", k, "=", v,
                          " for hidden service endpoint ", conf.first);
          return false;
        }
      }
      // start
      if(service->Start())
      {
        llarp::LogInfo("added hidden service endpoint ", service->Name());
        m_Endpoints.insert(std::make_pair(conf.first, std::move(service)));
        return true;
      }
      llarp::LogError("failed to start hidden service endpoint ", conf.first);
      return false;
    }
  }  // namespace service
}  // namespace llarp
