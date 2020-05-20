#include <numeric>
#include <peerstats/peer_db.hpp>

#include <catch2/catch.hpp>

TEST_CASE("Test PeerStats operator+=", "[PeerStats]")
{
  // TODO: test all members
  llarp::PeerStats stats;
  stats.numConnectionAttempts = 1;
  stats.peakBandwidthBytesPerSec = 12;

  llarp::PeerStats delta;
  delta.numConnectionAttempts = 2;
  delta.peakBandwidthBytesPerSec = 4;

  stats += delta;

  CHECK(stats.numConnectionAttempts == 3);
  CHECK(stats.peakBandwidthBytesPerSec == 12);  // should take max(), not add
}

TEST_CASE("Test PeerDb PeerStats memory storage", "[PeerDb]")
{
  const llarp::PeerStats empty = {};
  const llarp::RouterID id = {};

  llarp::PeerDb db;
  CHECK(db.getCurrentPeerStats(id) == empty);

  llarp::PeerStats delta;
  delta.numConnectionAttempts = 4;
  delta.peakBandwidthBytesPerSec = 5;
  db.accumulatePeerStats(id, delta);
  CHECK(db.getCurrentPeerStats(id) == delta);

  delta = {};
  delta.numConnectionAttempts = 5;
  delta.peakBandwidthBytesPerSec = 6;
  db.accumulatePeerStats(id, delta);

  llarp::PeerStats expected;
  expected.numConnectionAttempts = 9;
  expected.peakBandwidthBytesPerSec = 6;
  CHECK(db.getCurrentPeerStats(id) == expected);
}
