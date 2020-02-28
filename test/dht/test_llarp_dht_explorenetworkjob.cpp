#include <dht/explorenetworkjob.hpp>

#include <dht/messages/findrouter.hpp>
#include <dht/mock_context.hpp>
#include <test_util.hpp>

#include <gtest/gtest.h>

using namespace llarp;
using namespace ::testing;

using test::makeBuf;

struct TestDhtExploreNetworkJob : public ::testing::Test
{
  RouterID peer;
  test::MockContext context;
  dht::ExploreNetworkJob exploreNetworkJob;

  TestDhtExploreNetworkJob()
      : peer(makeBuf< RouterID >(0x01)), exploreNetworkJob(peer, &context)
  {
  }
};

TEST_F(TestDhtExploreNetworkJob, validate)
{
  const RouterID other = makeBuf< RouterID >(0x02);
  ASSERT_TRUE(exploreNetworkJob.Validate(other));
}

TEST_F(TestDhtExploreNetworkJob, start)
{
  // Verify input arguments are passed correctly.
  // The actual logic is inside the `dht::AbstractContext` implementation.

  const auto txKey = makeBuf< dht::Key_t >(0x02);
  uint64_t txId    = 4;

  dht::TXOwner txOwner(txKey, txId);

  // clang-format off
  EXPECT_CALL(context, DHTSendTo(
    Eq(txKey.as_array()),
    WhenDynamicCastTo< dht::FindRouterMessage* >(NotNull()),
    true)
  ).Times(1);
  // clang-format off

  ASSERT_NO_THROW(exploreNetworkJob.Start(txOwner));
}

TEST_F(TestDhtExploreNetworkJob, send_reply)
{
  // Concerns:
  // - Empty collection
  // - Lookup router fails (returns false)
  // - Number of calls matches collection size

  {
    exploreNetworkJob.valuesFound.clear();
    EXPECT_CALL(context, LookupRouter(_, _)).Times(0);
    EXPECT_CALL(context, GetRouter()).WillOnce(Return(nullptr));

    ASSERT_NO_THROW(exploreNetworkJob.SendReply());
  }

  {
    exploreNetworkJob.valuesFound.clear();
    exploreNetworkJob.valuesFound.push_back(makeBuf<RouterID>(0x00));
    exploreNetworkJob.valuesFound.push_back(makeBuf<RouterID>(0x01));
    exploreNetworkJob.valuesFound.push_back(makeBuf<RouterID>(0x02));

    EXPECT_CALL(context, GetRouter()).WillOnce(Return(nullptr));
    EXPECT_CALL(context, LookupRouter(Ne(makeBuf<RouterID>(0x01)), _)).Times(2).WillRepeatedly(Return(true));
    EXPECT_CALL(context, LookupRouter(Eq(makeBuf<RouterID>(0x01)), _)).WillOnce(Return(false));

    ASSERT_NO_THROW(exploreNetworkJob.SendReply());
  }

  {
    exploreNetworkJob.valuesFound.clear();
    exploreNetworkJob.valuesFound.push_back(makeBuf<RouterID>(0x00));
    exploreNetworkJob.valuesFound.push_back(makeBuf<RouterID>(0x01));
    exploreNetworkJob.valuesFound.push_back(makeBuf<RouterID>(0x02));

    EXPECT_CALL(context, GetRouter()).WillOnce(Return(nullptr));
    EXPECT_CALL(context, LookupRouter(_, _)).Times(3).WillRepeatedly(Return(true));

    ASSERT_NO_THROW(exploreNetworkJob.SendReply());
  }
}
