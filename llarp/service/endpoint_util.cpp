#include <service/endpoint_util.hpp>

#include <exit/session.hpp>
#include <service/outbound_context.hpp>
#include <service/lookup.hpp>
#include <util/logging/logger.hpp>

namespace llarp
{
  namespace service
  {
    void
    EndpointUtil::ExpireSNodeSessions(llarp_time_t now, SNodeSessions& sessions)
    {
      auto itr = sessions.begin();
      while (itr != sessions.end())
      {
        if (itr->second.first->ShouldRemove() && itr->second.first->IsStopped())
        {
          itr = sessions.erase(itr);
          continue;
        }
        // expunge next tick
        if (itr->second.first->IsExpired(now))
        {
          itr->second.first->Stop();
        }
        else
        {
          itr->second.first->Tick(now);
        }

        ++itr;
      }
    }

    void
    EndpointUtil::ExpirePendingTx(llarp_time_t now, PendingLookups& lookups)
    {
      for (auto itr = lookups.begin(); itr != lookups.end();)
      {
        if (!itr->second->IsTimedOut(now))
        {
          ++itr;
          continue;
        }
        std::unique_ptr<IServiceLookup> lookup = std::move(itr->second);

        LogWarn(lookup->name, " timed out txid=", lookup->txid);
        lookup->HandleResponse({});
        itr = lookups.erase(itr);
      }
    }

    void
    EndpointUtil::ExpirePendingRouterLookups(llarp_time_t now, PendingRouters& routers)
    {
      for (auto itr = routers.begin(); itr != routers.end();)
      {
        if (!itr->second.IsExpired(now))
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
    EndpointUtil::DeregisterDeadSessions(llarp_time_t now, Sessions& sessions)
    {
      auto itr = sessions.begin();
      while (itr != sessions.end())
      {
        if (itr->second->IsDone(now))
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
    EndpointUtil::TickRemoteSessions(
        llarp_time_t now, Sessions& remoteSessions, Sessions& deadSessions, ConvoMap& sessions)
    {
      auto itr = remoteSessions.begin();
      while (itr != remoteSessions.end())
      {
        itr->second->Tick(now);
        if (itr->second->Pump(now))
        {
          LogInfo("marking session as dead T=", itr->first);
          itr->second->Stop();
          sessions.erase(itr->second->currentConvoTag);
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
    EndpointUtil::ExpireConvoSessions(llarp_time_t now, ConvoMap& sessions)
    {
      auto itr = sessions.begin();
      while (itr != sessions.end())
      {
        if (itr->second.IsExpired(now))
        {
          LogInfo("Expire session T=", itr->first);
          itr = sessions.erase(itr);
        }
        else
          ++itr;
      }
    }

    void
    EndpointUtil::StopRemoteSessions(Sessions& remoteSessions)
    {
      for (auto& item : remoteSessions)
      {
        item.second->Stop();
      }
    }

    void
    EndpointUtil::StopSnodeSessions(SNodeSessions& sessions)
    {
      for (auto& item : sessions)
      {
        item.second.first->Stop();
      }
    }

    bool
    EndpointUtil::HasPathToService(const Address& addr, const Sessions& remoteSessions)
    {
      auto range = remoteSessions.equal_range(addr);
      auto itr = range.first;
      while (itr != range.second)
      {
        if (itr->second->ReadyToSend())
          return true;
        ++itr;
      }
      return false;
    }

    bool
    EndpointUtil::GetConvoTagsForService(
        const ConvoMap& sessions, const Address& info, std::set<ConvoTag>& tags)
    {
      bool inserted = false;
      auto itr = sessions.begin();
      while (itr != sessions.end())
      {
        if (itr->second.remote.Addr() == info)
        {
          if (tags.emplace(itr->first).second)
          {
            inserted = true;
          }
        }
        ++itr;
      }
      return inserted;
    }
  }  // namespace service
}  // namespace llarp
