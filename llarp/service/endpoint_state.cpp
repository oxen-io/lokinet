#include "endpoint_state.hpp"

#include <llarp/exit/session.hpp>
#include "endpoint.hpp"
#include "outbound_context.hpp"
#include <llarp/util/str.hpp>

namespace llarp
{
  namespace service
  {
    bool
    EndpointState::Configure(const NetworkConfig& conf)
    {
      if (conf.m_keyfile.has_value())
        m_Keyfile = conf.m_keyfile->string();
      m_SnodeBlacklist = conf.m_snodeBlacklist;
      m_ExitEnabled = conf.m_AllowExit;

      for (const auto& record : conf.m_SRVRecords)
      {
        m_IntroSet.SRVs.push_back(record.tuple);
      }

      return true;
    }

    util::StatusObject
    EndpointState::ExtractStatus(util::StatusObject& obj) const
    {
      obj["lastPublished"] = to_json(m_LastPublish);
      obj["lastPublishAttempt"] = to_json(m_LastPublishAttempt);
      obj["introset"] = m_IntroSet.ExtractStatus();
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
          [](const auto& item) { return item.second->ExtractStatus(); });

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
