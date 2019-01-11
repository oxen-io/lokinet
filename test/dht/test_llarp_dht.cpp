#include <dht/bucket.hpp>
#include <dht/key.hpp>
#include <dht/node.hpp>

#include <gtest/gtest.h>

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

TEST_F(KademliaDHTTest, TestBucketOperators)
{
  llarp::dht::Key_t zero;
  llarp::dht::Key_t one;
  llarp::dht::Key_t three;

  zero.Zero();
  one.Fill(1);
  three.Fill(3);
  ASSERT_TRUE(zero < one);
  ASSERT_TRUE(zero < three);
  ASSERT_FALSE(zero > one);
  ASSERT_FALSE(zero > three);
  ASSERT_TRUE(zero != three);
  ASSERT_FALSE(zero == three);
  ASSERT_TRUE((zero ^ one) == one);
  ASSERT_TRUE(one < three);
  ASSERT_TRUE(three > one);
  ASSERT_TRUE(one != three);
  ASSERT_FALSE(one == three);
  ASSERT_TRUE((one ^ three) == (three ^ one));
};

TEST_F(KademliaDHTTest, TestBucketRandomized_1000)
{
  size_t moreNodes = 100;
  while(moreNodes--)
  {
    llarp::dht::RCNode n;
    n.ID.Randomize();
    nodes->PutNode(n);
  }
  const size_t count = 1000;
  size_t left        = count;
  while(left--)
  {
    llarp::dht::Key_t result;
    llarp::dht::Key_t target;
    llarp::dht::Key_t expect;
    target.Randomize();
    expect = target;
    ASSERT_TRUE(nodes->FindClosest(target, result));
    if(target == result)
    {
      ASSERT_FALSE((result ^ target) < (expect ^ target));
      ASSERT_FALSE((result ^ target) != (expect ^ target));
      ASSERT_TRUE((result ^ target) == (expect ^ target));
    }
    else
    {
      Key_t dist    = result ^ target;
      Key_t oldDist = expect ^ target;
      ASSERT_TRUE((result ^ target) != (expect ^ target));
      if((result ^ target) < (expect ^ target))
      {
        std::cout << "result=" << result << "expect=" << expect << std::endl;
        std::cout << dist << ">=" << oldDist << "iteration=" << (count - left)
                  << std::endl;
        ASSERT_TRUE(false);
      }
      ASSERT_FALSE((result ^ target) == (expect ^ target));
    }
  }
};
