#include <numeric>
#include <peerstats/types.hpp>
#include <test_util.hpp>

#include <catch2/catch.hpp>

TEST_CASE("Test PeerStats operator+=", "[PeerStats]")
{
  llarp::RouterID id = {};

  // TODO: test all members
  llarp::PeerStats stats(id);
  stats.numConnectionAttempts = 1;
  stats.peakBandwidthBytesPerSec = 12;

  llarp::PeerStats delta(id);
  delta.numConnectionAttempts = 2;
  delta.peakBandwidthBytesPerSec = 4;

  stats += delta;

  CHECK(stats.numConnectionAttempts == 3);
  CHECK(stats.peakBandwidthBytesPerSec == 12);  // should take max(), not add
}

TEST_CASE("Test PeerStats BEncode", "[PeerStats]")
{
  llarp::RouterID id = llarp::test::makeBuf<llarp::RouterID>(0x01);

  llarp::PeerStats stats(id);

  stats.numConnectionAttempts = 1;
  stats.numConnectionSuccesses = 2;
  stats.numConnectionRejections = 3;
  stats.numConnectionTimeouts = 4;
  stats.numPathBuilds = 5;
  stats.numPacketsAttempted = 6;
  stats.numPacketsSent = 7;
  stats.numPacketsDropped = 8;
  stats.numPacketsResent = 9;
  stats.numDistinctRCsReceived = 10;
  stats.numLateRCs = 11;
  stats.peakBandwidthBytesPerSec = 12.1;  // should truncate to 12
  stats.longestRCReceiveInterval = 13ms;
  stats.leastRCRemainingLifetime = 14ms;
  stats.lastRCUpdated = 15ms;

  constexpr size_t bufSize = 4096;
  std::array<byte_t, bufSize> raw{};
  llarp_buffer_t buf(raw);

  CHECK_NOTHROW(stats.BEncode(&buf));

  std::string asString = (const char*)raw.data();
  constexpr std::string_view expected =
      "d"
      "13:lastRCUpdated"
      "i15e"
      "24:leastRCRemainingLifetime"
      "i14e"
      "24:longestRCReceiveInterval"
      "i13e"
      "21:numConnectionAttempts"
      "i1e"
      "23:numConnectionRejections"
      "i3e"
      "22:numConnectionSuccesses"
      "i2e"
      "21:numConnectionTimeouts"
      "i4e"
      "22:numDistinctRCsReceived"
      "i10e"
      "10:numLateRCs"
      "i11e"
      "19:numPacketsAttempted"
      "i6e"
      "17:numPacketsDropped"
      "i8e"
      "16:numPacketsResent"
      "i9e"
      "14:numPacketsSent"
      "i7e"
      "13:numPathBuilds"
      "i5e"
      "24:peakBandwidthBytesPerSec"
      "i12e"
      "e";

  CHECK(asString == expected);
}
