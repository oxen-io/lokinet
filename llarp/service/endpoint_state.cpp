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
    EndpointState::Configure(SnappConfig conf)
    {
      m_Keyfile = std::move(conf.m_keyfile);
      m_Tag = std::move(conf.m_tag);
      m_PrefetchTags = std::move(conf.m_prefetchTags);
      m_PrefetchAddrs = std::move(conf.m_prefetchAddrs);
      m_MinPathLatency = conf.m_minLatency;
      m_BundleRC = conf.m_bundleRC;

      // TODO: update SnappConfig to treat these as RouterIDs and detect dupes
      for (const auto& item : conf.m_snodeBlacklist)
      {
        RouterID snode;
        if (not snode.FromString(item))
          throw std::runtime_error(stringify("Invalide RouterID: ", item));

        m_SnodeBlacklist.insert(snode);
      }

      // TODO:
      /*
      if (k == "on-up")
      {
        m_OnUp = hooks::ExecShellBackend(v);
        if (m_OnUp)
          LogInfo(name, " added on up script: ", v);
        else
          LogError(name, " failed to add on up script");
      }
      if (k == "on-down")
      {
        m_OnDown = hooks::ExecShellBackend(v);
        if (m_OnDown)
          LogInfo(name, " added on down script: ", v);
        else
          LogError(name, " failed to add on down script");
      }
      if (k == "on-ready")
      {
        m_OnReady = hooks::ExecShellBackend(v);
        if (m_OnReady)
          LogInfo(name, " added on ready script: ", v);
        else
          LogError(name, " failed to add on ready script");
      }
      */
      return true;
    }

    util::StatusObject
    EndpointState::ExtractStatus(util::StatusObject& obj) const
    {
      obj["lastPublished"] = to_json(m_LastPublish);
      obj["lastPublishAttempt"] = to_json(m_LastPublishAttempt);
      obj["introset"] = m_IntroSet.ExtractStatus();

      if (!m_Tag.IsZero())
      {
        obj["tag"] = m_Tag.ToString();
      }

      static auto getSecond = [](const auto& item) -> auto
      {
        return item.second->ExtractStatus();
      };

      std::transform(
          m_DeadSessions.begin(),
          m_DeadSessions.end(),
          std::back_inserter(obj["deadSessions"]),
          getSecond);
      std::transform(
          m_RemoteSessions.begin(),
          m_RemoteSessions.end(),
          std::back_inserter(obj["remoteSessions"]),
          getSecond);
      std::transform(
          m_PendingLookups.begin(),
          m_PendingLookups.end(),
          std::back_inserter(obj["lookups"]),
          getSecond);
      std::transform(
          m_SNodeSessions.begin(),
          m_SNodeSessions.end(),
          std::back_inserter(obj["snodeSessions"]),
          [](const auto& item) { return item.second.first->ExtractStatus(); });

      util::StatusObject sessionObj{};

      for (const auto& item : m_Sessions)
      {
        std::string k = item.first.ToHex();
        sessionObj[k] = item.second.ExtractStatus();
      }

      obj["converstations"] = sessionObj;
      return obj;
    }
  }  // namespace service
}  // namespace llarp
