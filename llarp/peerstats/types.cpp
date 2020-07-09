#include <peerstats/types.hpp>

#include <util/str.hpp>

#include <stdexcept>

namespace llarp
{

  constexpr auto RouterIdKey = "routerId";
  constexpr auto NumConnectionAttemptsKey = "numConnectionAttempts";
  constexpr auto NumConnectionSuccessesKey = "numConnectionSuccesses";
  constexpr auto NumConnectionRejectionsKey = "numConnectionRejections";
  constexpr auto NumConnectionTimeoutsKey = "numConnectionTimeouts";
  constexpr auto NumPathBuildsKey = "numPathBuilds";
  constexpr auto NumPacketsAttemptedKey = "numPacketsAttempted";
  constexpr auto NumPacketsSentKey = "numPacketsSent";
  constexpr auto NumPacketsDroppedKey = "numPacketsDropped";
  constexpr auto NumPacketsResentKey = "numPacketsResent";
  constexpr auto NumDistinctRCsReceivedKey = "numDistinctRCsReceived";
  constexpr auto NumLateRCsKey = "numLateRCs";
  constexpr auto PeakBandwidthBytesPerSecKey = "peakBandwidthBytesPerSec";
  constexpr auto LongestRCReceiveIntervalKey = "longestRCReceiveInterval";
  constexpr auto LeastRCRemainingLifetimeKey = "leastRCRemainingLifetime";
  constexpr auto LastRCUpdatedKey = "lastRCUpdated";

  PeerStats::PeerStats() = default;

  PeerStats::PeerStats(const RouterID& routerId_) : routerId(routerId_)
  {
  }

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
    leastRCRemainingLifetime = std::max(leastRCRemainingLifetime, other.leastRCRemainingLifetime);
    lastRCUpdated = std::max(lastRCUpdated, other.lastRCUpdated);

    return *this;
  }

  bool
  PeerStats::operator==(const PeerStats& other) const
  {
    return routerId == other.routerId and numConnectionAttempts == other.numConnectionAttempts
        and numConnectionSuccesses == other.numConnectionSuccesses
        and numConnectionRejections == other.numConnectionRejections
        and numConnectionTimeouts == other.numConnectionTimeouts

        and numPathBuilds == other.numPathBuilds
        and numPacketsAttempted == other.numPacketsAttempted
        and numPacketsSent == other.numPacketsSent and numPacketsDropped == other.numPacketsDropped
        and numPacketsResent == other.numPacketsResent

        and numDistinctRCsReceived == other.numDistinctRCsReceived
        and numLateRCs == other.numLateRCs

        and peakBandwidthBytesPerSec == other.peakBandwidthBytesPerSec
        and longestRCReceiveInterval == other.longestRCReceiveInterval
        and leastRCRemainingLifetime == other.leastRCRemainingLifetime
        and lastRCUpdated == other.lastRCUpdated;
  }

  util::StatusObject
  PeerStats::toJson() const
  {
    return {
        {RouterIdKey, routerId.ToString()},
        {NumConnectionAttemptsKey, numConnectionAttempts},
        {NumConnectionSuccessesKey, numConnectionSuccesses},
        {NumConnectionRejectionsKey, numConnectionRejections},
        {NumConnectionTimeoutsKey, numConnectionTimeouts},
        {NumPathBuildsKey, numPathBuilds},
        {NumPacketsAttemptedKey, numPacketsAttempted},
        {NumPacketsSentKey, numPacketsSent},
        {NumPacketsDroppedKey, numPacketsDropped},
        {NumPacketsResentKey, numPacketsResent},
        {NumDistinctRCsReceivedKey, numDistinctRCsReceived},
        {NumLateRCsKey, numLateRCs},
        {PeakBandwidthBytesPerSecKey, peakBandwidthBytesPerSec},
        {LongestRCReceiveIntervalKey, longestRCReceiveInterval.count()},
        {LeastRCRemainingLifetimeKey, leastRCRemainingLifetime.count()},
        {LastRCUpdatedKey, lastRCUpdated.count()},
    };
  }

  void
  PeerStats::BEncode(llarp_buffer_t* buf)
  {
    if (not buf)
      throw std::runtime_error("PeerStats: Can't use null buf");

    auto encodeUint64Entry = [&](std::string_view key, uint64_t value) {
      if (not bencode_write_uint64_entry(buf, key.data(), key.size(), value))
        throw std::runtime_error(stringify("PeerStats: Could not encode ", key));
    };

    if (not bencode_start_dict(buf))
      throw std::runtime_error("PeerStats: Could not create bencode dict");

    // TODO: we don't have bencode support for dict entries other than uint64...?

    // encodeUint64Entry(RouterIdKey, routerId);
    encodeUint64Entry(NumConnectionAttemptsKey, numConnectionAttempts);
    encodeUint64Entry(NumConnectionSuccessesKey, numConnectionSuccesses);
    encodeUint64Entry(NumConnectionRejectionsKey, numConnectionRejections);
    encodeUint64Entry(NumConnectionTimeoutsKey, numConnectionTimeouts);
    encodeUint64Entry(NumPathBuildsKey, numPathBuilds);
    encodeUint64Entry(NumPacketsAttemptedKey, numPacketsAttempted);
    encodeUint64Entry(NumPacketsSentKey, numPacketsSent);
    encodeUint64Entry(NumPacketsDroppedKey, numPacketsDropped);
    encodeUint64Entry(NumPacketsResentKey, numPacketsResent);
    encodeUint64Entry(NumDistinctRCsReceivedKey, numDistinctRCsReceived);
    encodeUint64Entry(NumLateRCsKey, numLateRCs);
    encodeUint64Entry(PeakBandwidthBytesPerSecKey, (uint64_t)peakBandwidthBytesPerSec);
    encodeUint64Entry(LongestRCReceiveIntervalKey, longestRCReceiveInterval.count());
    encodeUint64Entry(LeastRCRemainingLifetimeKey, leastRCRemainingLifetime.count());
    encodeUint64Entry(LastRCUpdatedKey, lastRCUpdated.count());

    if (not bencode_end(buf))
      throw std::runtime_error("PeerStats: Could not end bencode dict");

  }

};  // namespace llarp
