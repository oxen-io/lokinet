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
      auto itr = m_Endpoints.begin();
      while(itr != m_Endpoints.end())
      {
        itr->second->Tick();
        ++itr;
      }
    }

    bool
    Context::AddEndpoint(const Config::section_t &conf)
    {
      auto itr = m_Endpoints.find(conf.first);
      if(itr != m_Endpoints.end())
      {
        llarp::LogError("cannot add hidden service with duplicate name: ",
                        conf.first);
        return false;
      }
      auto service = new llarp::service::Endpoint(conf.first, m_Router);
      for(const auto &option : conf.second)
      {
        auto &k = option.first;
        auto &v = option.second;
        if(!service->SetOption(k, v))
        {
          llarp::LogError("failed to set ", k, "=", v,
                          " for hidden service endpoint ", conf.first);
          delete service;
          return false;
        }
      }
      if(service->Start())
      {
        llarp::LogInfo("added hidden service endpoint ", conf.first);
        m_Endpoints.emplace(conf.first, service);
        return true;
      }
      llarp::LogError("failed to start hidden service endpoint ", conf.first);
      delete service;
      return false;
    }
  }  // namespace service
}  // namespace llarp