#include "endpoint_state.hpp"

#include <llarp/exit/session.hpp>
#include "endpoint.hpp"
#include "outbound_context.hpp"
#include <llarp/util/str.hpp>

namespace llarp::service
{
  bool
  EndpointState::Configure(const NetworkConfig& conf)
  {
    if (conf.m_keyfile.has_value())
      key_file = conf.m_keyfile->string();
    snode_blacklist = conf.m_snodeBlacklist;
    is_exit_enabled = conf.m_AllowExit;

    for (const auto& record : conf.m_SRVRecords)
    {
      local_introset.SRVs.push_back(record.toTuple());
    }

    return true;
  }

  util::StatusObject
  EndpointState::ExtractStatus(util::StatusObject& obj) const
  {
    obj["lastPublished"] = to_json(last_publish);
    obj["lastPublishAttempt"] = to_json(last_publish_attempt);
    obj["introset"] = local_introset.ExtractStatus();
    // static auto getSecond = [](const auto& item) -> auto
    // {
    //   return item.second.ExtractStatus();
    // };

    // std::transform(
    //     dead_sessions.begin(),
    //     dead_sessions.end(),
    //     std::back_inserter(obj["deadSessions"]),
    //     getSecond);
    // std::transform(
    //     remote_sessions.begin(),
    //     remote_sessions.end(),
    //     std::back_inserter(obj["remoteSessions"]),
    //     getSecond);
    // std::transform(
    //     snode_sessions.begin(),
    //     snode_sessions.end(),
    //     std::back_inserter(obj["snodeSessions"]),
    //     [](const auto& item) { return item.second->ExtractStatus(); });

    util::StatusObject sessionObj{};

    // TODO:
    // for (const auto& item : m_Sessions)
    // {
    //   std::string k = item.first.ToHex();
    //   sessionObj[k] = item.second.ExtractStatus();
    // }

    obj["converstations"] = sessionObj;
    return obj;
  }
}  // namespace llarp::service
