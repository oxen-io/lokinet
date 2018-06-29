#include <gtest/gtest.h>
#include <llarp/dht.hpp>

using Key_t = llarp::dht::Key_t;

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
    nodes           = new llarp::dht::Bucket< llarp::dht::RCNode >(us);
    size_t numNodes = 10;
    byte_t fill     = 1;
    while(numNodes)
    {
      llarp::dht::RCNode n;
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

  llarp::dht::Bucket< llarp::dht::RCNode >* nodes = nullptr;
  llarp::dht::Key_t us;
};

TEST_F(KademliaDHTTest, TestBucketFindClosest)
{
  llarp::dht::Key_t result;
  llarp::dht::Key_t target;
  llarp::dht::Key_t oldResult;
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
    llarp::dht::RCNode n;
    n.ID.Randomize();
    nodes->PutNode(n);
  }
  llarp::dht::Key_t result;
  llarp::dht::Key_t target;
  llarp::dht::Key_t oldResult;
  target.Randomize();
  ASSERT_TRUE(nodes->FindClosest(target, result));
};
