#pragma once

#include <sqlite_orm/sqlite_orm.h>

#include <peerstats/types.hpp>

/// Contains some code to help deal with sqlite_orm in hopes of keeping other headers clean

namespace llarp
{
  inline auto
  initStorage(const std::string& file)
  {
    using namespace sqlite_orm;
    return make_storage(
        file,
        make_table(
            "peerstats",
            make_column("routerId", &PeerStats::routerId, primary_key(), unique()),
            make_column("numConnectionAttempts", &PeerStats::numConnectionAttempts),
            make_column("numConnectionSuccesses", &PeerStats::numConnectionSuccesses),
            make_column("numConnectionRejections", &PeerStats::numConnectionRejections),
            make_column("numConnectionTimeouts", &PeerStats::numConnectionTimeouts),
            make_column("numPathBuilds", &PeerStats::numPathBuilds),
            make_column("numPacketsAttempted", &PeerStats::numPacketsAttempted),
            make_column("numPacketsSent", &PeerStats::numPacketsSent),
            make_column("numPacketsDropped", &PeerStats::numPacketsDropped),
            make_column("numPacketsResent", &PeerStats::numPacketsResent),
            make_column("numDistinctRCsReceived", &PeerStats::numDistinctRCsReceived),
            make_column("numLateRCs", &PeerStats::numLateRCs),
            make_column("peakBandwidthBytesPerSec", &PeerStats::peakBandwidthBytesPerSec),
            make_column("longestRCReceiveIntervalMs", &PeerStats::longestRCReceiveIntervalMs),
            make_column("mostExpiredRCMs", &PeerStats::mostExpiredRCMs)));
  }

  using PeerDbStorage = decltype(initStorage(""));

}  // namespace llarp
