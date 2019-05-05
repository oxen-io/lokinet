#include <service/endpoint_util.hpp>

#include <service/outbound_context.hpp>
#include <util/logger.hpp>

namespace llarp
{
  namespace service
  {
    void
    EndpointUtil::ExpireSNodeSessions(llarp_time_t now,
                                      Endpoint::SNodeSessions& sessions)
    {
      auto itr = sessions.begin();
      while(itr != sessions.end())
      {
        if(itr->second->ShouldRemove() && itr->second->IsStopped())
        {
          itr = sessions.erase(itr);
          continue;
        }
        // expunge next tick
        if(itr->second->IsExpired(now))
        {
          itr->second->Stop();
        }
        else
        {
          itr->second->Tick(now);
        }

        ++itr;
      }
    }

    void
    EndpointUtil::ExpirePendingTx(llarp_time_t now,
                                  Endpoint::PendingLookups& lookups)
    {
      for(auto itr = lookups.begin(); itr != lookups.end();)
      {
        if(!itr->second->IsTimedOut(now))
        {
          ++itr;
          continue;
        }
        std::unique_ptr< IServiceLookup > lookup = std::move(itr->second);

        LogWarn(lookup->name, " timed out txid=", lookup->txid);
        lookup->HandleResponse({});
        itr = lookups.erase(itr);
      }
    }

    void
    EndpointUtil::ExpirePendingRouterLookups(llarp_time_t now,
                                             Endpoint::PendingRouters& routers)
    {
      for(auto itr = routers.begin(); itr != routers.end();)
      {
        if(!itr->second.IsExpired(now))
        {
          ++itr;
          continue;
        }
        LogWarn("lookup for ", itr->first, " timed out");
        itr->second.InformResult({});
        itr = routers.erase(itr);
      }
    }

    void
    EndpointUtil::DeregisterDeadSessions(llarp_time_t now,
                                         Endpoint::Sessions& sessions)
    {
      auto itr = sessions.begin();
      while(itr != sessions.end())
      {
        if(itr->second->IsDone(now))
        {
          itr = sessions.erase(itr);
        }
        else
        {
          ++itr;
        }
      }
    }

    void
    EndpointUtil::TickRemoteSessions(llarp_time_t now,
                                     Endpoint::Sessions& remoteSessions,
                                     Endpoint::Sessions& deadSessions)
    {
      auto itr = remoteSessions.begin();
      while(itr != remoteSessions.end())
      {
        itr->second->Tick(now);
        if(itr->second->Pump(now))
        {
          itr->second->Stop();
          deadSessions.emplace(std::move(*itr));
          itr = remoteSessions.erase(itr);
        }
        else
        {
          ++itr;
        }
      }
    }

    void
    EndpointUtil::ExpireConvoSessions(llarp_time_t now,
                                      Endpoint::ConvoMap& sessions)
    {
      auto itr = sessions.begin();
      while(itr != sessions.end())
      {
        if(itr->second.IsExpired(now))
          itr = sessions.erase(itr);
        else
          ++itr;
      }
    }

    void
    EndpointUtil::StopRemoteSessions(Endpoint::Sessions& remoteSessions)
    {
      for(auto& item : remoteSessions)
      {
        item.second->Stop();
      }
    }

    void
    EndpointUtil::StopSnodeSessions(Endpoint::SNodeSessions& sessions)
    {
      for(auto& item : sessions)
      {
        item.second->Stop();
      }
    }

    bool
    EndpointUtil::HasPathToService(const Address& addr,
                                   const Endpoint::Sessions& remoteSessions)
    {
      auto range = remoteSessions.equal_range(addr);
      auto itr   = range.first;
      while(itr != range.second)
      {
        if(itr->second->ReadyToSend())
          return true;
        ++itr;
      }
      return false;
    }
  }  // namespace service
}  // namespace llarp
