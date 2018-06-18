#include <gtest/gtest.h>
#include <llarp/dht.hpp>

using Node   = llarp::dht::Node;
using Key_t  = llarp::dht::Key_t;
using Bucket = llarp::dht::Bucket;

class KademliaDHTTest : public ::testing::Test
{
 public:
  KademliaDHTTest()
  {
  }
  ~KademliaDHTTest()
  {
  }

  void
  SetUp()
  {
    us.Fill(16);
    nodes           = new Bucket(us);
    size_t numNodes = 10;
    byte_t fill     = 1;
    while(numNodes)
    {
      Node n;
      n.ID.Fill(fill);
      nodes->PutNode(n);
      --numNodes;
      ++fill;
    }
  }

  void
  TearDown()
  {
    delete nodes;
  }

  Bucket* nodes = nullptr;
  Key_t us;
};

TEST_F(KademliaDHTTest, TestBucketFindClosest)
{
  Key_t result;
  Key_t target;
  Key_t oldResult;
  target.Fill(5);
  ASSERT_TRUE(nodes->FindClosest(target, result));
  ASSERT_TRUE(target == result);
  oldResult = result;
  target.Fill(0xf5);
  ASSERT_TRUE(nodes->FindClosest(target, result));
  ASSERT_TRUE(oldResult == result);
};

TEST_F(KademliaDHTTest, TestBucketRandomzied)
{
  size_t moreNodes = 100;
  while(moreNodes--)
  {
    Node n;
    n.ID.Randomize();
    nodes->PutNode(n);
  }
  Key_t result;
  Key_t target;
  Key_t oldResult;
  target.Randomize();
  ASSERT_TRUE(nodes->FindClosest(target, result));
};
