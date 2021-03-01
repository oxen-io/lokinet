#include <dht/bucket.hpp>
#include <dht/key.hpp>
#include <dht/node.hpp>

#include <catch2/catch.hpp>

using Key_t = llarp::dht::Key_t;
using Value_t = llarp::dht::RCNode;
using Bucket_t = llarp::dht::Bucket<Value_t>;

class TestDhtBucket
{
 public:
  TestDhtBucket() : randInt(0)
  {
    us.Fill(16);
    nodes = std::make_unique<Bucket_t>(us, [&]() { return randInt++; });
    size_t numNodes = 10;
    byte_t fill = 1;
    while (numNodes)
    {
      Value_t n;
      n.ID.Fill(fill);
      nodes->PutNode(n);
      --numNodes;
      ++fill;
    }
  }

  uint64_t randInt;

  llarp::dht::Key_t us;
  std::unique_ptr<Bucket_t> nodes;
};

TEST_CASE_METHOD(TestDhtBucket, "Simple cycle", "[dht]")
{
  // Empty the current bucket.
  nodes->Clear();

  // Create a simple value, and add it to the bucket.
  Value_t val;
  val.ID.Fill(1);

  nodes->PutNode(val);

  // Verify the value is in the bucket
  REQUIRE(nodes->HasNode(val.ID));
  REQUIRE(1u == nodes->size());

  // Verify after deletion, the value is no longer in the bucket
  nodes->DelNode(val.ID);
  REQUIRE_FALSE(nodes->HasNode(val.ID));

  // Verify deleting again succeeds;
  nodes->DelNode(val.ID);
  REQUIRE_FALSE(nodes->HasNode(val.ID));
}

TEST_CASE_METHOD(TestDhtBucket, "get_random_node_excluding")
{
  // Empty the current bucket.
  nodes->Clear();

  // We expect not to find anything
  Key_t result;
  std::set<Key_t> excludeSet;
  REQUIRE_FALSE(nodes->GetRandomNodeExcluding(result, excludeSet));

  // Create a simple value.
  Value_t val;
  val.ID.Fill(1);

  // Add the simple value to the exclude set
  excludeSet.insert(val.ID);
  REQUIRE_FALSE(nodes->GetRandomNodeExcluding(result, excludeSet));

  // Add the simple value to the bucket
  nodes->PutNode(val);
  REQUIRE_FALSE(nodes->GetRandomNodeExcluding(result, excludeSet));

  excludeSet.clear();

  REQUIRE(nodes->GetRandomNodeExcluding(result, excludeSet));
  REQUIRE(val.ID == result);

  // Add an element to the exclude set which isn't the bucket.
  Key_t other;
  other.Fill(0xff);
  excludeSet.insert(other);

  REQUIRE(nodes->GetRandomNodeExcluding(result, excludeSet));
  REQUIRE(val.ID == result);

  // Add a node which is in both bucket and excludeSet
  Value_t nextVal;
  nextVal.ID.Fill(0xAA);
  excludeSet.insert(nextVal.ID);
  nodes->PutNode(nextVal);

  REQUIRE(nodes->GetRandomNodeExcluding(result, excludeSet));
  REQUIRE(val.ID == result);

  // Clear the excludeSet - we should still have 2 nodes in the bucket
  excludeSet.clear();

  randInt = 0;
  REQUIRE(nodes->GetRandomNodeExcluding(result, excludeSet));
  REQUIRE(val.ID == result);

  // Set the random value to be 1, we should get the other node.
  randInt = 1;
  REQUIRE(nodes->GetRandomNodeExcluding(result, excludeSet));
  REQUIRE(nextVal.ID == result);

  // Set the random value to be 100, we should get the first node.
  randInt = 100;
  REQUIRE(nodes->GetRandomNodeExcluding(result, excludeSet));
  REQUIRE(val.ID == result);
}

TEST_CASE_METHOD(TestDhtBucket, "find_closest", "[dht]")
{
  // Empty the current bucket.
  nodes->Clear();

  // We expect not to find anything
  Key_t target;
  target.Fill(0xF0);

  Key_t result;
  REQUIRE_FALSE(nodes->FindClosest(target, result));

  // Add a node to the bucket
  Value_t first;
  first.ID.Zero();
  nodes->PutNode(first);

  REQUIRE(nodes->FindClosest(target, result));
  REQUIRE(result == first.ID);

  // Add another node to the bucket, closer to the target
  Value_t second;
  second.ID.Fill(0x10);
  nodes->PutNode(second);
  REQUIRE(nodes->FindClosest(target, result));
  REQUIRE(result == second.ID);

  // Add a third node to the bucket, closer to the target
  Value_t third;
  third.ID.Fill(0x20);
  nodes->PutNode(third);
  REQUIRE(nodes->FindClosest(target, result));
  REQUIRE(result == third.ID);

  // Add a fourth node to the bucket, greater than the target
  Value_t fourth;
  fourth.ID.Fill(0xF1);
  nodes->PutNode(fourth);
  REQUIRE(nodes->FindClosest(target, result));
  REQUIRE(result == fourth.ID);

  // Add a fifth node to the bucket, equal to the target
  Value_t fifth;
  fifth.ID.Fill(0xF0);
  nodes->PutNode(fifth);
  REQUIRE(nodes->FindClosest(target, result));
  REQUIRE(result == fifth.ID);
}

TEST_CASE_METHOD(TestDhtBucket, "get_many_random", "[dht]")
{
  // Empty the current bucket.
  nodes->Clear();

  // Verify behaviour with empty node set
  std::set<Key_t> result;
  REQUIRE_FALSE(nodes->GetManyRandom(result, 0));
  REQUIRE_FALSE(nodes->GetManyRandom(result, 1));

  // Add 5 nodes to the bucket
  std::set<Value_t> curValues;
  std::set<Key_t> curKeys;
  for (byte_t i = 0x00; i < 0x05; ++i)
  {
    Value_t v;
    v.ID.Fill(i);
    REQUIRE(curKeys.insert(v.ID).second);
    nodes->PutNode(v);
  }

  // Fetching more than the current size fails
  REQUIRE(5u == nodes->size());
  REQUIRE_FALSE(nodes->GetManyRandom(result, nodes->size() + 1));

  // Fetching the current size succeeds
  REQUIRE(nodes->GetManyRandom(result, nodes->size()));
  REQUIRE(curKeys == result);

  // Fetching a subset succeeds.
  // Note we hack this by "fixing" the random number generator
  result.clear();

  REQUIRE(nodes->GetManyRandom(result, 1u));
  REQUIRE(1u == result.size());
  REQUIRE(*curKeys.begin() == *result.begin());

  randInt = 0;
  result.clear();

  REQUIRE(nodes->GetManyRandom(result, nodes->size() - 1));
  REQUIRE(nodes->size() - 1 == result.size());
  REQUIRE(std::set<Key_t>(++curKeys.rbegin(), curKeys.rend()) == result);
}

TEST_CASE_METHOD(TestDhtBucket, "find_close_excluding", "[dht]")
{
  // Empty the current bucket.
  nodes->Clear();

  Key_t target;
  target.Zero();
  std::set<Key_t> exclude;
  Key_t result;

  // Empty node + exclude set fails
  REQUIRE_FALSE(nodes->FindCloseExcluding(target, result, exclude));

  Value_t first;
  first.ID.Fill(0xF0);
  exclude.insert(first.ID);

  // Empty nodes fails
  REQUIRE_FALSE(nodes->FindCloseExcluding(target, result, exclude));

  // Nodes and exclude set match
  nodes->PutNode(first);
  REQUIRE_FALSE(nodes->FindCloseExcluding(target, result, exclude));

  // Exclude set empty
  exclude.clear();
  REQUIRE(nodes->FindCloseExcluding(target, result, exclude));
  result = first.ID;

  Value_t second;
  second.ID.Fill(0x01);
  nodes->PutNode(second);

  REQUIRE(nodes->FindCloseExcluding(target, result, exclude));
  result = second.ID;

  exclude.insert(second.ID);
  REQUIRE(nodes->FindCloseExcluding(target, result, exclude));
  result = first.ID;
}

TEST_CASE_METHOD(TestDhtBucket, "find_many_near_excluding", "[dht]")
{
  // Empty the current bucket.
  nodes->Clear();

  Key_t target;
  target.Zero();
  std::set<Key_t> exclude;
  std::set<Key_t> result;

  // Empty node + exclude set, with size 0 succeeds
  REQUIRE(nodes->GetManyNearExcluding(target, result, 0, exclude));
  REQUIRE(0u == result.size());
  // Empty node + exclude set fails
  REQUIRE_FALSE(nodes->GetManyNearExcluding(target, result, 1, exclude));

  Value_t first;
  first.ID.Fill(0xF0);
  exclude.insert(first.ID);

  // Empty nodes fails
  REQUIRE_FALSE(nodes->GetManyNearExcluding(target, result, 1, exclude));

  // Nodes and exclude set match
  nodes->PutNode(first);
  REQUIRE_FALSE(nodes->GetManyNearExcluding(target, result, 1, exclude));

  // Single node succeeds
  exclude.clear();
  REQUIRE(nodes->GetManyNearExcluding(target, result, 1, exclude));
  REQUIRE(result == std::set<Key_t>({first.ID}));

  // Trying to grab 2 nodes from a 1 node set fails
  result.clear();
  REQUIRE_FALSE(nodes->GetManyNearExcluding(target, result, 2, exclude));

  // two nodes finds closest
  Value_t second;
  second.ID.Fill(0x01);
  nodes->PutNode(second);
  result.clear();
  REQUIRE(nodes->GetManyNearExcluding(target, result, 1, exclude));
  REQUIRE(result == std::set<Key_t>({second.ID}));

  // 3 nodes finds 2 closest
  Value_t third;
  third.ID.Fill(0x02);
  nodes->PutNode(third);
  result.clear();
  REQUIRE(nodes->GetManyNearExcluding(target, result, 2, exclude));
  REQUIRE(result == std::set<Key_t>({second.ID, third.ID}));

  // 4 nodes, one in exclude set finds 2 closest
  Value_t fourth;
  fourth.ID.Fill(0x03);
  nodes->PutNode(fourth);
  exclude.insert(third.ID);
  result.clear();
  REQUIRE(nodes->GetManyNearExcluding(target, result, 2, exclude));
  REQUIRE(result == std::set<Key_t>({second.ID, fourth.ID}));
}

TEST_CASE_METHOD(TestDhtBucket, "Bucket: FindClosest", "[dht]")
{
  llarp::dht::Key_t result;
  llarp::dht::Key_t target;
  target.Fill(5);
  REQUIRE(nodes->FindClosest(target, result));
  REQUIRE(target == result);
  const llarp::dht::Key_t oldResult = result;
  target.Fill(0xf5);
  REQUIRE(nodes->FindClosest(target, result));
  REQUIRE(oldResult == result);
}

TEST_CASE_METHOD(TestDhtBucket, "Bucket: randomized 1000", "[dht]")
{
  size_t moreNodes = 100;
  while (moreNodes--)
  {
    llarp::dht::RCNode n;
    n.ID.Fill(randInt);
    randInt++;
    nodes->PutNode(n);
  }
  const size_t count = 1000;
  size_t left = count;
  while (left--)
  {
    llarp::dht::Key_t result;
    llarp::dht::Key_t target;
    target.Randomize();
    const llarp::dht::Key_t expect = target;
    REQUIRE(nodes->FindClosest(target, result));
    if (target == result)
    {
      REQUIRE((result ^ target) >= (expect ^ target));
      REQUIRE((result ^ target) == (expect ^ target));
      REQUIRE((result ^ target) == (expect ^ target));
    }
    else
    {
      Key_t dist = (result ^ target);
      Key_t oldDist = (expect ^ target);
      REQUIRE((result ^ target) != (expect ^ target));

      INFO(dist << ">=" << oldDist << "iteration=" << (count - left));
      REQUIRE((result ^ target) >= (expect ^ target));

      REQUIRE((result ^ target) != (expect ^ target));
    }
  }
}
