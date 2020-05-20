#pragma once

#include <chrono>
#include <unordered_map>

#include <sqlite_orm/sqlite_orm.h>

#include <router_id.hpp>
#include <util/time.hpp>

namespace llarp
{
  // Struct containing stats we know about a peer
  struct PeerStats
  {
    int32_t numConnectionAttempts = 0;
    int32_t numConnectionSuccesses = 0;
    int32_t numConnectionRejections = 0;
    int32_t numConnectionTimeouts = 0;

    int32_t numPathBuilds = 0;
    int64_t numPacketsAttempted = 0;
    int64_t numPacketsSent = 0;
    int64_t numPacketsDropped = 0;
    int64_t numPacketsResent = 0;

    int64_t numDistinctRCsReceived = 0;
    int64_t numLateRCs = 0;

    double peakBandwidthBytesPerSec = 0;
    std::chrono::milliseconds longestRCReceiveInterval = 0ms;
    std::chrono::milliseconds mostExpiredRC = 0ms;

    PeerStats&
    operator+=(const PeerStats& other);
    bool
    operator==(const PeerStats& other);
  };

  /// Maintains a database of stats collected about the connections with our Service Node peers
  struct PeerDb
  {
    /// Add the given stats to the cummulative stats for the given peer. For cummulative stats, the
    /// stats are added together; for watermark stats, the max is kept.
    ///
    /// This is intended to be used in the following pattern:
    ///
    /// 1) Initialize an empty PeerStats
    /// 2) Collect relevant stats
    /// 3) Call accumulatePeerStats() with the stats
    /// 4) Reset the stats to 0
    /// 5) <Repeat 2-4 periodically>
    void
    accumulatePeerStats(const RouterID& routerId, const PeerStats& delta);

    /// Provides a snapshot of the most recent PeerStats we have for the given peer. If we don't
    /// have any stats for the peer, an empty PeerStats is returned.
    ///
    /// @param routerId is the RouterID of the requested peer
    /// @return a copy of the most recent peer stats or an empty one if no such peer is known
    PeerStats
    getCurrentPeerStats(const RouterID& routerId) const;

   private:
    std::unordered_map<RouterID, PeerStats, RouterID::Hash> m_peerStats;
    std::mutex m_statsLock;
  };

}  // namespace llarp
