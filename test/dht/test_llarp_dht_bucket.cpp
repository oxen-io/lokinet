#include <dht/bucket.hpp>
#include <dht/key.hpp>
#include <dht/node.hpp>

#include <gtest/gtest.h>

using Key_t    = llarp::dht::Key_t;
using Value_t  = llarp::dht::RCNode;
using Bucket_t = llarp::dht::Bucket< Value_t >;

class TestDhtBucket : public ::testing::Test
{
 public:
  TestDhtBucket() : randInt(0)
  {
    us.Fill(16);
    nodes = std::make_unique< Bucket_t >(us, [&]() { return randInt++; });
    size_t numNodes = 10;
    byte_t fill     = 1;
    while(numNodes)
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
  std::unique_ptr< Bucket_t > nodes;
};

TEST_F(TestDhtBucket, simple_cycle)
{
  // Empty the current bucket.
  nodes->Clear();

  // Create a simple value, and add it to the bucket.
  Value_t val;
  val.ID.Fill(1);

  nodes->PutNode(val);

  // Verify the value is in the bucket
  ASSERT_TRUE(nodes->HasNode(val.ID));
  ASSERT_EQ(1u, nodes->size());

  // Verify after deletion, the value is no longer in the bucket
  nodes->DelNode(val.ID);
  ASSERT_FALSE(nodes->HasNode(val.ID));

  // Verify deleting again succeeds;
  nodes->DelNode(val.ID);
  ASSERT_FALSE(nodes->HasNode(val.ID));
}

TEST_F(TestDhtBucket, get_random_node_excluding)
{
  // Empty the current bucket.
  nodes->Clear();

  // We expect not to find anything
  Key_t result;
  std::set< Key_t > excludeSet;
  ASSERT_FALSE(nodes->GetRandomNodeExcluding(result, excludeSet));

  // Create a simple value.
  Value_t val;
  val.ID.Fill(1);

  // Add the simple value to the exclude set
  excludeSet.insert(val.ID);
  ASSERT_FALSE(nodes->GetRandomNodeExcluding(result, excludeSet));

  // Add the simple value to the bucket
  nodes->PutNode(val);
  ASSERT_FALSE(nodes->GetRandomNodeExcluding(result, excludeSet));

  excludeSet.clear();

  ASSERT_TRUE(nodes->GetRandomNodeExcluding(result, excludeSet));
  ASSERT_EQ(val.ID, result);

  // Add an element to the exclude set which isn't the bucket.
  Key_t other;
  other.Fill(0xff);
  excludeSet.insert(other);

  ASSERT_TRUE(nodes->GetRandomNodeExcluding(result, excludeSet));
  ASSERT_EQ(val.ID, result);

  // Add a node which is in both bucket and excludeSet
  Value_t nextVal;
  nextVal.ID.Fill(0xAA);
  excludeSet.insert(nextVal.ID);
  nodes->PutNode(nextVal);

  ASSERT_TRUE(nodes->GetRandomNodeExcluding(result, excludeSet));
  ASSERT_EQ(val.ID, result);

  // Clear the excludeSet - we should still have 2 nodes in the bucket
  excludeSet.clear();

  randInt = 0;
  ASSERT_TRUE(nodes->GetRandomNodeExcluding(result, excludeSet));
  ASSERT_EQ(val.ID, result);

  // Set the random value to be 1, we should get the other node.
  randInt = 1;
  ASSERT_TRUE(nodes->GetRandomNodeExcluding(result, excludeSet));
  ASSERT_EQ(nextVal.ID, result);

  // Set the random value to be 100, we should get the first node.
  randInt = 100;
  ASSERT_TRUE(nodes->GetRandomNodeExcluding(result, excludeSet));
  ASSERT_EQ(val.ID, result);
}

TEST_F(TestDhtBucket, find_closest)
{
  // Empty the current bucket.
  nodes->Clear();

  // We expect not to find anything
  Key_t target;
  target.Fill(0xF0);

  Key_t result;
  ASSERT_FALSE(nodes->FindClosest(target, result));

  // Add a node to the bucket
  Value_t first;
  first.ID.Zero();
  nodes->PutNode(first);

  ASSERT_TRUE(nodes->FindClosest(target, result));
  ASSERT_EQ(result, first.ID);

  // Add another node to the bucket, closer to the target
  Value_t second;
  second.ID.Fill(0x10);
  nodes->PutNode(second);
  ASSERT_TRUE(nodes->FindClosest(target, result));
  ASSERT_EQ(result, second.ID);

  // Add a third node to the bucket, closer to the target
  Value_t third;
  third.ID.Fill(0x20);
  nodes->PutNode(third);
  ASSERT_TRUE(nodes->FindClosest(target, result));
  ASSERT_EQ(result, third.ID);

  // Add a fourth node to the bucket, greater than the target
  Value_t fourth;
  fourth.ID.Fill(0xF1);
  nodes->PutNode(fourth);
  ASSERT_TRUE(nodes->FindClosest(target, result));
  ASSERT_EQ(result, fourth.ID);

  // Add a fifth node to the bucket, equal to the target
  Value_t fifth;
  fifth.ID.Fill(0xF0);
  nodes->PutNode(fifth);
  ASSERT_TRUE(nodes->FindClosest(target, result));
  ASSERT_EQ(result, fifth.ID);
}

TEST_F(TestDhtBucket, get_many_random)
{
  // Empty the current bucket.
  nodes->Clear();

  // Verify behaviour with empty node set
  std::set< Key_t > result;
  ASSERT_FALSE(nodes->GetManyRandom(result, 0));
  ASSERT_FALSE(nodes->GetManyRandom(result, 1));

  // Add 5 nodes to the bucket
  std::set< Value_t > curValues;
  std::set< Key_t > curKeys;
  for(byte_t i = 0x00; i < 0x05; ++i)
  {
    Value_t v;
    v.ID.Fill(i);
    ASSERT_TRUE(curKeys.insert(v.ID).second);
    nodes->PutNode(v);
  }

  // Fetching more than the current size fails
  ASSERT_EQ(5u, nodes->size());
  ASSERT_FALSE(nodes->GetManyRandom(result, nodes->size() + 1));

  // Fetching the current size succeeds
  ASSERT_TRUE(nodes->GetManyRandom(result, nodes->size()));
  ASSERT_EQ(curKeys, result);

  // Fetching a subset succeeds.
  // Note we hack this by "fixing" the random number generator
  result.clear();

  ASSERT_TRUE(nodes->GetManyRandom(result, 1u));
  ASSERT_EQ(1u, result.size());
  ASSERT_EQ(*curKeys.begin(), *result.begin());

  randInt = 0;
  result.clear();

  ASSERT_TRUE(nodes->GetManyRandom(result, nodes->size() - 1));
  ASSERT_EQ(nodes->size() - 1, result.size());
  ASSERT_EQ(std::set< Key_t >(++curKeys.rbegin(), curKeys.rend()), result);
}

TEST_F(TestDhtBucket, find_close_excluding)
{
  // Empty the current bucket.
  nodes->Clear();

  Key_t target;
  target.Zero();
  std::set< Key_t > exclude;
  Key_t result;

  // Empty node + exclude set fails
  ASSERT_FALSE(nodes->FindCloseExcluding(target, result, exclude));

  Value_t first;
  first.ID.Fill(0xF0);
  exclude.insert(first.ID);

  // Empty nodes fails
  ASSERT_FALSE(nodes->FindCloseExcluding(target, result, exclude));

  // Nodes and exclude set match
  nodes->PutNode(first);
  ASSERT_FALSE(nodes->FindCloseExcluding(target, result, exclude));

  // Exclude set empty
  exclude.clear();
  ASSERT_TRUE(nodes->FindCloseExcluding(target, result, exclude));
  result = first.ID;

  Value_t second;
  second.ID.Fill(0x01);
  nodes->PutNode(second);

  ASSERT_TRUE(nodes->FindCloseExcluding(target, result, exclude));
  result = second.ID;

  exclude.insert(second.ID);
  ASSERT_TRUE(nodes->FindCloseExcluding(target, result, exclude));
  result = first.ID;
}

TEST_F(TestDhtBucket, find_many_near_excluding)
{
  // Empty the current bucket.
  nodes->Clear();

  Key_t target;
  target.Zero();
  std::set< Key_t > exclude;
  std::set< Key_t > result;

  // Empty node + exclude set, with size 0 succeeds
  ASSERT_TRUE(nodes->GetManyNearExcluding(target, result, 0, exclude));
  ASSERT_EQ(0u, result.size());
  // Empty node + exclude set fails
  ASSERT_FALSE(nodes->GetManyNearExcluding(target, result, 1, exclude));

  Value_t first;
  first.ID.Fill(0xF0);
  exclude.insert(first.ID);

  // Empty nodes fails
  ASSERT_FALSE(nodes->GetManyNearExcluding(target, result, 1, exclude));

  // Nodes and exclude set match
  nodes->PutNode(first);
  ASSERT_FALSE(nodes->GetManyNearExcluding(target, result, 1, exclude));

  // Single node succeeds
  exclude.clear();
  ASSERT_TRUE(nodes->GetManyNearExcluding(target, result, 1, exclude));
  ASSERT_EQ(result, std::set< Key_t >({first.ID}));

  // Trying to grab 2 nodes from a 1 node set fails
  result.clear();
  ASSERT_FALSE(nodes->GetManyNearExcluding(target, result, 2, exclude));

  // two nodes finds closest
  Value_t second;
  second.ID.Fill(0x01);
  nodes->PutNode(second);
  result.clear();
  ASSERT_TRUE(nodes->GetManyNearExcluding(target, result, 1, exclude));
  ASSERT_EQ(result, std::set< Key_t >({second.ID}));

  // 3 nodes finds 2 closest
  Value_t third;
  third.ID.Fill(0x02);
  nodes->PutNode(third);
  result.clear();
  ASSERT_TRUE(nodes->GetManyNearExcluding(target, result, 2, exclude));
  ASSERT_EQ(result, std::set< Key_t >({second.ID, third.ID}));

  // 4 nodes, one in exclude set finds 2 closest
  Value_t fourth;
  fourth.ID.Fill(0x03);
  nodes->PutNode(fourth);
  exclude.insert(third.ID);
  result.clear();
  ASSERT_TRUE(nodes->GetManyNearExcluding(target, result, 2, exclude));
  ASSERT_EQ(result, std::set< Key_t >({second.ID, fourth.ID}));
}

TEST_F(TestDhtBucket, TestBucketFindClosest)
{
  llarp::dht::Key_t result;
  llarp::dht::Key_t target;
  target.Fill(5);
  ASSERT_TRUE(nodes->FindClosest(target, result));
  ASSERT_EQ(target, result);
  const llarp::dht::Key_t oldResult = result;
  target.Fill(0xf5);
  ASSERT_TRUE(nodes->FindClosest(target, result));
  ASSERT_EQ(oldResult, result);
};

TEST_F(TestDhtBucket, TestBucketRandomized_1000)
{
  size_t moreNodes = 100;
  while(moreNodes--)
  {
    llarp::dht::RCNode n;
    n.ID.Fill(randInt);
    randInt++;
    nodes->PutNode(n);
  }
  const size_t count = 1000;
  size_t left        = count;
  while(left--)
  {
    llarp::dht::Key_t result;
    llarp::dht::Key_t target;
    target.Randomize();
    const llarp::dht::Key_t expect = target;
    ASSERT_TRUE(nodes->FindClosest(target, result));
    if(target == result)
    {
      ASSERT_GE(result ^ target, expect ^ target);
      ASSERT_EQ(result ^ target, expect ^ target);
      ASSERT_EQ(result ^ target, expect ^ target);
    }
    else
    {
      Key_t dist    = result ^ target;
      Key_t oldDist = expect ^ target;
      ASSERT_NE(result ^ target, expect ^ target);

      ASSERT_GE(result ^ target, expect ^ target)
          << "result=" << result << "expect=" << expect << std::endl
          << dist << ">=" << oldDist << "iteration=" << (count - left);

      ASSERT_NE(result ^ target, expect ^ target);
    }
  }
};
