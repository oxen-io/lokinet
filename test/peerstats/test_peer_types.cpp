#include <numeric>
#include <peerstats/types.hpp>

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
