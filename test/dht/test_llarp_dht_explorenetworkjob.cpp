#include <dht/explorenetworkjob.hpp>

#include <dht/messages/findrouter.hpp>
#include <dht/mock_context.hpp>
#include <test_util.hpp>

#include <gmock/gmock.h>
#include <catch2/catch.hpp>

using namespace llarp;
using namespace ::testing;

using test::makeBuf;

struct TestDhtExploreNetworkJob
{
  RouterID peer;
  test::MockContext context;
  dht::ExploreNetworkJob exploreNetworkJob;

  TestDhtExploreNetworkJob() : peer(makeBuf<RouterID>(0x01)), exploreNetworkJob(peer, &context)
  {}

  ~TestDhtExploreNetworkJob()
  {
    CHECK(Mock::VerifyAndClearExpectations(&context));
  }
};

TEST_CASE_METHOD(TestDhtExploreNetworkJob, "validate", "[dht]")
{
  const RouterID other = makeBuf<RouterID>(0x02);
  REQUIRE(exploreNetworkJob.Validate(other));
}

TEST_CASE_METHOD(TestDhtExploreNetworkJob, "start", "[dht]")
{
  // Verify input arguments are passed correctly.
  // The actual logic is inside the `dht::AbstractContext` implementation.

  const auto txKey = makeBuf<dht::Key_t>(0x02);
  uint64_t txId = 4;

  dht::TXOwner txOwner(txKey, txId);

  // clang-format off
  EXPECT_CALL(context, DHTSendTo(
    Eq(txKey.as_array()),
    WhenDynamicCastTo< dht::FindRouterMessage* >(NotNull()),
    true)
  ).Times(1);
  // clang-format off

  REQUIRE_NOTHROW(exploreNetworkJob.Start(txOwner));
}

// TODO: sections?
TEST_CASE_METHOD(TestDhtExploreNetworkJob, "send_reply", "[dht]")
{
  // Concerns:
  // - Empty collection
  // - Lookup router fails (returns false)
  // - Number of calls matches collection size

  {
    exploreNetworkJob.valuesFound.clear();
    EXPECT_CALL(context, LookupRouter(_, _)).Times(0);
    EXPECT_CALL(context, GetRouter()).WillOnce(Return(nullptr));

    REQUIRE_NOTHROW(exploreNetworkJob.SendReply());
  }

  {
    exploreNetworkJob.valuesFound.clear();
    exploreNetworkJob.valuesFound.push_back(makeBuf<RouterID>(0x00));
    exploreNetworkJob.valuesFound.push_back(makeBuf<RouterID>(0x01));
    exploreNetworkJob.valuesFound.push_back(makeBuf<RouterID>(0x02));

    EXPECT_CALL(context, GetRouter()).WillOnce(Return(nullptr));
    EXPECT_CALL(context, LookupRouter(Ne(makeBuf<RouterID>(0x01)), _)).Times(2).WillRepeatedly(Return(true));
    EXPECT_CALL(context, LookupRouter(Eq(makeBuf<RouterID>(0x01)), _)).WillOnce(Return(false));

    REQUIRE_NOTHROW(exploreNetworkJob.SendReply());
  }

  {
    exploreNetworkJob.valuesFound.clear();
    exploreNetworkJob.valuesFound.push_back(makeBuf<RouterID>(0x00));
    exploreNetworkJob.valuesFound.push_back(makeBuf<RouterID>(0x01));
    exploreNetworkJob.valuesFound.push_back(makeBuf<RouterID>(0x02));

    EXPECT_CALL(context, GetRouter()).WillOnce(Return(nullptr));
    EXPECT_CALL(context, LookupRouter(_, _)).Times(3).WillRepeatedly(Return(true));

    REQUIRE_NOTHROW(exploreNetworkJob.SendReply());
  }
}
