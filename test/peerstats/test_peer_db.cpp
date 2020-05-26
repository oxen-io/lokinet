#include <peerstats/peer_db.hpp>
#include <test_util.hpp>

#include <numeric>
#include <catch2/catch.hpp>

TEST_CASE("Test PeerDb PeerStats memory storage", "[PeerDb]")
{
  const llarp::RouterID id = llarp::test::makeBuf<llarp::RouterID>(0x01);
  const llarp::PeerStats empty(id);

  llarp::PeerDb db;
  CHECK(db.getCurrentPeerStats(id).has_value() == false);

  llarp::PeerStats delta(id);
  delta.numConnectionAttempts = 4;
  delta.peakBandwidthBytesPerSec = 5;
  db.accumulatePeerStats(id, delta);
  CHECK(db.getCurrentPeerStats(id).value() == delta);

  delta = llarp::PeerStats(id);
  delta.numConnectionAttempts = 5;
  delta.peakBandwidthBytesPerSec = 6;
  db.accumulatePeerStats(id, delta);

  llarp::PeerStats expected(id);
  expected.numConnectionAttempts = 9;
  expected.peakBandwidthBytesPerSec = 6;
  CHECK(db.getCurrentPeerStats(id).value() == expected);
}

TEST_CASE("Test PeerDb flush before load", "[PeerDb]")
{
  llarp::PeerDb db;
  CHECK_THROWS_WITH(db.flushDatabase(), "Cannot flush database before it has been loaded");
}

TEST_CASE("Test PeerDb load twice", "[PeerDb]")
{
  llarp::PeerDb db;
  CHECK_NOTHROW(db.loadDatabase(std::nullopt));
  CHECK_THROWS_WITH(db.loadDatabase(std::nullopt), "Reloading database not supported");
}

TEST_CASE("Test PeerDb nukes stats on load", "[PeerDb]")
{
  const llarp::RouterID id = llarp::test::makeBuf<llarp::RouterID>(0x01);

  llarp::PeerDb db;

  llarp::PeerStats stats(id);
  stats.numConnectionAttempts = 1;

  db.accumulatePeerStats(id, stats);
  CHECK(db.getCurrentPeerStats(id).value() == stats);

  db.loadDatabase(std::nullopt);

  CHECK(db.getCurrentPeerStats(id).has_value() == false);
}

TEST_CASE("Test PeerDb file-backed database reloads properly", "[PeerDb]")
{
  const std::string filename = "/tmp/peerdb_test_tmp2.db.sqlite";
  const llarp::RouterID id = llarp::test::makeBuf<llarp::RouterID>(0x02);

  {
    llarp::PeerDb db;
    db.loadDatabase(filename);

    llarp::PeerStats stats(id);
    stats.numConnectionAttempts = 43;

    db.accumulatePeerStats(id, stats);

    db.flushDatabase();
  }

  {
    llarp::PeerDb db;
    db.loadDatabase(filename);

    auto stats = db.getCurrentPeerStats(id);
    CHECK(stats.has_value() == true);
    CHECK(stats.value().numConnectionAttempts == 43);
  }

  fs::remove(filename);
}

TEST_CASE("Test PeerDb modifyPeerStats", "[PeerDb]")
{
  const llarp::RouterID id = llarp::test::makeBuf<llarp::RouterID>(0xF2);

  int numTimesCalled = 0;

  llarp::PeerDb db;
  db.loadDatabase(std::nullopt);

  db.modifyPeerStats(id, [&](llarp::PeerStats& stats) {
    numTimesCalled++;

    stats.numPathBuilds += 42;
  });

  db.flushDatabase();

  CHECK(numTimesCalled == 1);

  auto stats = db.getCurrentPeerStats(id);
  CHECK(stats.has_value());
  CHECK(stats.value().numPathBuilds == 42);
}
