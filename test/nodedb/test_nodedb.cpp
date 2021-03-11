#include <catch2/catch.hpp>
#include "config/config.hpp"

#include <router_contact.hpp>
#include <nodedb.hpp>

using llarp_nodedb = llarp::NodeDB;

TEST_CASE("FindClosestTo returns correct number of elements", "[nodedb][dht]")
{
  llarp_nodedb nodeDB{fs::current_path(), nullptr};

  constexpr uint64_t numRCs = 3;
  for (uint64_t i = 0; i < numRCs; ++i)
  {
    llarp::RouterContact rc;
    rc.pubkey[0] = i;
    nodeDB.Put(rc);
  }

  REQUIRE(numRCs == nodeDB.NumLoaded());

  llarp::dht::Key_t key;

  std::vector<llarp::RouterContact> results = nodeDB.FindManyClosestTo(key, 4);

  // we asked for more entries than nodedb had
  REQUIRE(numRCs == results.size());
}

TEST_CASE("FindClosestTo returns properly ordered set", "[nodedb][dht]")
{
  llarp_nodedb nodeDB{fs::current_path(), nullptr};

  // insert some RCs: a < b < c
  llarp::RouterContact a;
  a.pubkey[0] = 1;
  nodeDB.Put(a);

  llarp::RouterContact b;
  b.pubkey[0] = 2;
  nodeDB.Put(b);

  llarp::RouterContact c;
  c.pubkey[0] = 3;
  nodeDB.Put(c);

  REQUIRE(3 == nodeDB.NumLoaded());

  llarp::dht::Key_t key;

  std::vector<llarp::RouterContact> results = nodeDB.FindManyClosestTo(key, 2);
  REQUIRE(2 == results.size());

  // we xor'ed with 0x0, so order should be a,b,c
  REQUIRE(a.pubkey == results[0].pubkey);
  REQUIRE(b.pubkey == results[1].pubkey);

  llarp::dht::Key_t compKey;
  compKey.Fill(0xFF);

  results = nodeDB.FindManyClosestTo(compKey, 2);

  // we xor'ed with 0xF...F, so order should be inverted (c,b,a)
  REQUIRE(c.pubkey == results[0].pubkey);
  REQUIRE(b.pubkey == results[1].pubkey);
}
