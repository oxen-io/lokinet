#pragma once

#include <chrono>
#include <unordered_map>

#include <llarp/router_id.hpp>
#include <llarp/util/status.hpp>
#include <llarp/util/time.hpp>

/// Types stored in our peerstats database are declared here

namespace llarp
{
  // Struct containing stats we know about a peer
  struct PeerStats
  {
    RouterID routerId;

    int32_t numConnectionAttempts = 0;
    int32_t numConnectionSuccesses = 0;
    int32_t numConnectionRejections = 0;
    int32_t numConnectionTimeouts = 0;

    int32_t numPathBuilds = 0;
    int64_t numPacketsAttempted = 0;
    int64_t numPacketsSent = 0;
    int64_t numPacketsDropped = 0;
    int64_t numPacketsResent = 0;

    int32_t numDistinctRCsReceived = 0;
    int32_t numLateRCs = 0;

    double peakBandwidthBytesPerSec = 0;
    llarp_time_t longestRCReceiveInterval = 0ms;
    llarp_time_t leastRCRemainingLifetime = 0ms;
    llarp_time_t lastRCUpdated = 0ms;

    // not serialized
    bool stale = true;

    PeerStats();
    PeerStats(const RouterID& routerId);

    PeerStats&
    operator+=(const PeerStats& other);
    bool
    operator==(const PeerStats& other) const;

    util::StatusObject
    toJson() const;

    void
    BEncode(llarp_buffer_t* buf) const;

    static void
    BEncodeList(const std::vector<PeerStats>& statsList, llarp_buffer_t* buf);
  };

}  // namespace llarp
