#include <service/endpoint_state.hpp>

#include <exit/session.hpp>
#include <hook/shell.hpp>
#include <service/endpoint.hpp>
#include <service/outbound_context.hpp>
#include <util/str.hpp>

namespace llarp
{
  namespace service
  {
    bool
    EndpointState::SetOption(const std::string& k, const std::string& v,
                             Endpoint& ep)
    {
      const auto name = ep.Name();
      if(k == "keyfile")
      {
        m_Keyfile = v;
      }
      if(k == "tag")
      {
        m_Tag = v;
        LogInfo("Setting tag to ", v);
      }
      if(k == "prefetch-tag")
      {
        m_PrefetchTags.insert(v);
      }
      if(k == "prefetch-addr")
      {
        Address addr;
        if(addr.FromString(v))
          m_PrefetchAddrs.insert(addr);
      }
      if(k == "min-latency")
      {
        const auto val = atoi(v.c_str());
        if(val > 0)
          m_MinPathLatency = std::chrono::milliseconds(val);
      }
      if(k == "paths")
      {
        const auto val = atoi(v.c_str());
        if(val >= 1 && val <= static_cast< int >(path::PathSet::max_paths))
        {
          ep.numPaths = val;
          LogInfo(name, " set number of paths to ", ep.numHops);
        }
        else
        {
          LogWarn(name, " invalid number of paths: ", v);
        }
      }
      if(k == "hops")
      {
        const auto val = atoi(v.c_str());
        if(val >= 1 && val <= static_cast< int >(path::max_len))
        {
          ep.numHops = val;
          LogInfo(name, " set number of hops to ", ep.numHops);
        }
        else
        {
          LogWarn(name, " invalid number of hops: ", v);
        }
      }
      if(k == "bundle-rc")
      {
        m_BundleRC = IsTrueValue(v.c_str());
      }
      if(k == "blacklist-snode")
      {
        RouterID snode;
        if(!snode.FromString(v))
        {
          LogError(name, " invalid snode value: ", v);
          return false;
        }
        const auto result = m_SnodeBlacklist.insert(snode);
        if(!result.second)
        {
          LogError(name, " duplicate blacklist-snode: ", snode.ToString());
          return false;
        }
        LogInfo(name, " adding ", snode.ToString(), " to blacklist");
      }
      if(k == "on-up")
      {
        m_OnUp = hooks::ExecShellBackend(v);
        if(m_OnUp)
          LogInfo(name, " added on up script: ", v);
        else
          LogError(name, " failed to add on up script");
      }
      if(k == "on-down")
      {
        m_OnDown = hooks::ExecShellBackend(v);
        if(m_OnDown)
          LogInfo(name, " added on down script: ", v);
        else
          LogError(name, " failed to add on down script");
      }
      if(k == "on-ready")
      {
        m_OnReady = hooks::ExecShellBackend(v);
        if(m_OnReady)
          LogInfo(name, " added on ready script: ", v);
        else
          LogError(name, " failed to add on ready script");
      }
      return true;
    }

    util::StatusObject
    EndpointState::ExtractStatus(util::StatusObject& obj) const
    {
      obj["lastPublished"]      = to_json(m_LastPublish);
      obj["lastPublishAttempt"] = to_json(m_LastPublishAttempt);
      obj["introset"]           = m_IntroSet.ExtractStatus();

      if(!m_Tag.IsZero())
      {
        obj["tag"] = m_Tag.ToString();
      }

      static auto getSecond = [](const auto& item) -> auto
      {
        return item.second->ExtractStatus();
      };

      std::transform(m_DeadSessions.begin(), m_DeadSessions.end(),
                     std::back_inserter(obj["deadSessions"]), getSecond);
      std::transform(m_RemoteSessions.begin(), m_RemoteSessions.end(),
                     std::back_inserter(obj["remoteSessions"]), getSecond);
      std::transform(m_PendingLookups.begin(), m_PendingLookups.end(),
                     std::back_inserter(obj["lookups"]), getSecond);
      std::transform(
          m_SNodeSessions.begin(), m_SNodeSessions.end(),
          std::back_inserter(obj["snodeSessions"]),
          [](const auto& item) { return item.second.first->ExtractStatus(); });

      util::StatusObject sessionObj{};

      for(const auto& item : m_Sessions)
      {
        std::string k = item.first.ToHex();
        sessionObj[k] = item.second.ExtractStatus();
      }

      obj["converstations"] = sessionObj;
      return obj;
    }
  }  // namespace service
}  // namespace llarp
