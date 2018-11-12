#include <llarp/exit/context.hpp>

namespace llarp
{
  namespace exit
  {
    Context::Context(llarp_router* r) : m_Router(r)
    {
    }
    Context::~Context()
    {
    }

    void
    Context::Tick(llarp_time_t now)
    {
      auto itr = m_Exits.begin();
      while(itr != m_Exits.end())
      {
        itr->second->Tick(now);
        ++itr;
      }
    }

    bool
    Context::AddExitEndpoint(const std::string& name, const Config_t& conf)
    {
      // check for duplicate exit by name
      {
        auto itr = m_Exits.find(name);
        if(itr != m_Exits.end())
        {
          llarp::LogError("duplicate exit with name ", name);
          return false;
        }
      }
      std::unique_ptr< llarp::handlers::ExitEndpoint > endpoint;
      // make new endpoint
      endpoint.reset(new llarp::handlers::ExitEndpoint(name, m_Router));
      // configure
      {
        auto itr = conf.begin();
        while(itr != conf.end())
        {
          if(!endpoint->SetOption(itr->first, itr->second))
            return false;
          ++itr;
        }
      }
      // add endpoint
      m_Exits.emplace(name, std::move(endpoint));
      return true;
    }

  }  // namespace exit
}  // namespace llarp