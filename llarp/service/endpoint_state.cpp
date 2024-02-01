#include "endpoint_state.hpp"

namespace llarp::service
{
    bool EndpointState::Configure(const NetworkConfig& conf)
    {
        if (conf.keyfile.has_value())
            key_file = conf.keyfile->string();
        snode_blacklist = conf.snode_blacklist;
        is_exit_enabled = conf.allow_exit;

        for (const auto& record : conf.srv_records)
        {
            local_introset.SRVs.push_back(record.toTuple());
        }

        return true;
    }

    util::StatusObject EndpointState::ExtractStatus(util::StatusObject& obj) const
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
