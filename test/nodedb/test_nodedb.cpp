#include <catch2/catch.hpp>
#include "config/config.hpp"

#include <router_contact.hpp>
#include <nodedb.hpp>

#include <random>
#include <set>
#include <system_error>

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

namespace
{
  struct TempNodeDB
  {
    fs::path root;
    llarp_nodedb db;

    // Runs disk jobs inline so a Tick's writes are on disk when it returns.
    TempNodeDB()
        : root{fs::temp_directory_path()
               / ("lokinet-nodedb-test-" + std::to_string(std::random_device{}()))}
        , db{root, [](std::function<void()> f) { f(); }}
    {}

    ~TempNodeDB()
    {
      std::error_code ec;
      fs::remove_all(root, ec);
    }

    std::set<fs::path>
    files() const
    {
      std::set<fs::path> found;
      for (const auto& entry : fs::recursive_directory_iterator{root})
        if (entry.is_regular_file())
          found.insert(entry.path());
      return found;
    }

    void
    wipe()
    {
      for (const auto& f : files())
        fs::remove(f);
    }

    // Each call lands past the next flush deadline, so every call flushes.
    void
    flush()
    {
      flushes += 1;
      db.Tick(llarp::time_now_ms() + flushes * 6min);
    }

    int flushes = 0;
  };

  llarp::RouterContact
  make_rc(uint8_t id, llarp_time_t updated)
  {
    llarp::RouterContact rc;
    rc.pubkey[0] = id;
    rc.last_updated = updated;
    return rc;
  }
}  // namespace

TEST_CASE("NodeDB flush writes RCs stored since the last flush", "[nodedb]")
{
  TempNodeDB t;
  t.db.Put(make_rc(1, 1000s));
  t.db.PutIfNewer(make_rc(2, 1000s));

  t.flush();

  REQUIRE(t.files().size() == 2);
}

TEST_CASE("NodeDB flush skips RCs that have not changed", "[nodedb]")
{
  TempNodeDB t;
  t.db.Put(make_rc(1, 1000s));
  t.flush();
  t.wipe();

  t.flush();

  REQUIRE(t.files().empty());
}

TEST_CASE("NodeDB Put of an identical RC does not rewrite it", "[nodedb]")
{
  TempNodeDB t;
  t.db.Put(make_rc(1, 1000s));
  t.flush();
  t.wipe();

  t.db.Put(make_rc(1, 1000s));
  t.flush();

  REQUIRE(t.files().empty());
  REQUIRE(t.db.NumLoaded() == 1);
}

TEST_CASE("NodeDB Put of a changed RC rewrites only that RC", "[nodedb]")
{
  TempNodeDB t;
  t.db.Put(make_rc(1, 1000s));
  t.db.Put(make_rc(2, 1000s));
  t.flush();
  t.wipe();

  t.db.Put(make_rc(1, 2000s));
  t.flush();

  REQUIRE(t.files().size() == 1);
  REQUIRE(t.db.Get(make_rc(1, 0s).pubkey)->last_updated == 2000s);
}

TEST_CASE("NodeDB PutIfNewer rewrites only for a newer RC", "[nodedb]")
{
  TempNodeDB t;
  t.db.PutIfNewer(make_rc(1, 2000s));
  t.flush();
  t.wipe();

  SECTION("an older RC is neither stored nor written")
  {
    t.db.PutIfNewer(make_rc(1, 1000s));
    t.flush();
    REQUIRE(t.files().empty());
    REQUIRE(t.db.Get(make_rc(1, 0s).pubkey)->last_updated == 2000s);
  }

  SECTION("the same RC is not written again")
  {
    t.db.PutIfNewer(make_rc(1, 2000s));
    t.flush();
    REQUIRE(t.files().empty());
  }

  SECTION("a newer RC is written")
  {
    t.db.PutIfNewer(make_rc(1, 3000s));
    t.flush();
    REQUIRE(t.files().size() == 1);
  }
}

TEST_CASE("NodeDB does not write an RC removed before the flush", "[nodedb]")
{
  TempNodeDB t;
  t.db.Put(make_rc(1, 1000s));
  t.db.Put(make_rc(2, 1000s));
  t.db.Remove(make_rc(1, 0s).pubkey);

  t.flush();

  REQUIRE(t.files().size() == 1);
}

TEST_CASE("NodeDB does not flush before the flush interval", "[nodedb]")
{
  TempNodeDB t;
  t.db.Put(make_rc(1, 1000s));

  t.db.Tick(llarp::time_now_ms());

  REQUIRE(t.files().empty());
}

TEST_CASE("NodeDB writes an RC stored after a flush at the next one", "[nodedb]")
{
  TempNodeDB t;
  t.db.Put(make_rc(1, 1000s));
  t.flush();
  t.wipe();

  t.db.Put(make_rc(2, 1000s));
  t.flush();

  REQUIRE(t.files().size() == 1);
}
