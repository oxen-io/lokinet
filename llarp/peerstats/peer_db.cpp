#include <peerstats/peer_db.hpp>

namespace llarp
{
  PeerStats&
  PeerStats::operator+=(const PeerStats& other)
  {
    numConnectionAttempts += other.numConnectionAttempts;
    numConnectionSuccesses += other.numConnectionSuccesses;
    numConnectionRejections += other.numConnectionRejections;
    numConnectionTimeouts += other.numConnectionTimeouts;

    numPathBuilds += other.numPathBuilds;
    numPacketsAttempted += other.numPacketsAttempted;
    numPacketsSent += other.numPacketsSent;
    numPacketsDropped += other.numPacketsDropped;
    numPacketsResent += other.numPacketsResent;

    numDistinctRCsReceived += other.numDistinctRCsReceived;
    numLateRCs += other.numLateRCs;

    peakBandwidthBytesPerSec = std::max(peakBandwidthBytesPerSec, other.peakBandwidthBytesPerSec);
    longestRCReceiveInterval = std::max(longestRCReceiveInterval, other.longestRCReceiveInterval);
    mostExpiredRC = std::max(mostExpiredRC, other.mostExpiredRC);

    return *this;
  }

  bool
  PeerStats::operator==(const PeerStats& other)
  {
    return numConnectionAttempts == other.numConnectionAttempts
        and numConnectionSuccesses == other.numConnectionSuccesses
        and numConnectionRejections == other.numConnectionRejections
        and numConnectionTimeouts == other.numConnectionTimeouts

        and numPathBuilds == other.numPathBuilds
        and numPacketsAttempted == other.numPacketsAttempted
        and numPacketsSent == other.numPacketsSent and numPacketsDropped == other.numPacketsDropped
        and numPacketsResent == other.numPacketsResent

        and numDistinctRCsReceived == other.numDistinctRCsReceived
        and numLateRCs == other.numLateRCs

        and peakBandwidthBytesPerSec == peakBandwidthBytesPerSec
        and longestRCReceiveInterval == longestRCReceiveInterval and mostExpiredRC == mostExpiredRC;
  }

  void
  PeerDb::accumulatePeerStats(const RouterID& routerId, const PeerStats& delta)
  {
    std::lock_guard gaurd(m_statsLock);
    m_peerStats[routerId] += delta;
  }

  PeerStats
  PeerDb::getCurrentPeerStats(const RouterID& routerId) const
  {
    std::lock_guard gaurd(m_statsLock);
    auto itr = m_peerStats.find(routerId);
    if (itr == m_peerStats.end())
      return {};
    else
      return itr->second;
  }

};  // namespace llarp
