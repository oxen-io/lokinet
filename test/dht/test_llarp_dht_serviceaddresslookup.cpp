#include <dht/serviceaddresslookup.hpp>

#include <crypto/mock_crypto.hpp>
#include <dht/mock_context.hpp>
#include <dht/messages/gotintro.hpp>
#include <llarp_test.hpp>
#include <service/intro_set.hpp>
#include <test_util.hpp>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

using namespace llarp;
using namespace ::testing;

using test::makeBuf;

struct MockIntroSetHandler
{
  MOCK_METHOD1(call, void(const std::vector< service::IntroSet > &));
};

static constexpr uint64_t EXPIRY = 1548503831ull;

struct TestDhtServiceAddressLookup : public test::LlarpTest<>
{
  MockIntroSetHandler introsetHandler;

  dht::Key_t ourKey;
  dht::Key_t txKey;
  uint64_t txId;
  dht::TXOwner txOwner;
  service::Address address;

  test::MockContext context;
  uint64_t r;
  std::unique_ptr< dht::ServiceAddressLookup > serviceAddressLookup;

  TestDhtServiceAddressLookup()
      : ourKey(makeBuf< dht::Key_t >(0xFF))
      , txKey(makeBuf< dht::Key_t >(0x01))
      , txId(2)
      , txOwner(txKey, txId)
      , address(makeBuf< service::Address >(0x03))
      , r(4)
  {
    EXPECT_CALL(context, OurKey()).WillOnce(ReturnRef(ourKey));

    serviceAddressLookup = std::make_unique< dht::ServiceAddressLookup >(
        txOwner, address, &context, r,
        std::bind(&MockIntroSetHandler::call, &introsetHandler,
                  std::placeholders::_1));
  }
};

TEST_F(TestDhtServiceAddressLookup, validate)
{
  // Concerns
  // - introset fails to verify
  // - introset topic is not the target
  // - happy path

  {
    service::IntroSet introset;
    EXPECT_CALL(context, Now()).WillOnce(Return(EXPIRY));
    EXPECT_CALL(m_crypto, verify(_, _, _)).WillOnce(Return(false));

    ASSERT_FALSE(serviceAddressLookup->Validate(introset));
  }

  {
    service::IntroSet introset;

    // Fiddle with the introset so we pass the Verify call
    introset.I.emplace_back();
    introset.I.front().expiresAt =
        EXPIRY + service::MAX_INTROSET_TIME_DELTA + 1;

    // Set expectations
    EXPECT_CALL(context, Now()).WillOnce(Return(EXPIRY));
    EXPECT_CALL(m_crypto, verify(_, _, _)).WillOnce(Return(true));

    ASSERT_FALSE(serviceAddressLookup->Validate(introset));
  }

  {
    service::IntroSet introset;
    // Set the current address of the lookup to be equal to the default address.
    // This is easier than manipulating the ServiceInfo address.
    serviceAddressLookup->target.Zero();

    // Fiddle with the introset so we pass the Verify call
    introset.I.emplace_back();
    introset.I.front().expiresAt =
        EXPIRY + service::MAX_INTROSET_TIME_DELTA + 1;

    // Set expectations
    EXPECT_CALL(context, Now()).WillOnce(Return(EXPIRY));
    EXPECT_CALL(m_crypto, verify(_, _, _)).WillOnce(Return(true));

    ASSERT_FALSE(serviceAddressLookup->Validate(introset));
  }
}

TEST_F(TestDhtServiceAddressLookup, start)
{
  // Verify input arguments are passed correctly.
  // The actual logic is inside the `dht::AbstractContext` implementation.

  // clang-format off
  EXPECT_CALL(context, DHTSendTo(
    Eq(txKey.as_array()),
    WhenDynamicCastTo< dht::FindIntroMessage* >(NotNull()),
    true)
  ).Times(1);
  // clang-format off

  ASSERT_NO_THROW(serviceAddressLookup->Start(txOwner));
}

TEST_F(TestDhtServiceAddressLookup, get_next_peer)
{
    // Concerns
  // - Nodes returns nullptr
  // - Happy path

  dht::Key_t key = makeBuf< dht::Key_t >(0x02);
  std::set< dht::Key_t > exclude;
  {
    EXPECT_CALL(context, Nodes()).WillOnce(ReturnNull());
    ASSERT_FALSE(serviceAddressLookup->GetNextPeer(key, exclude));
  }

  {
    uint64_t randVal = 0;

    dht::Bucket< dht::RCNode > nodes(ourKey, [&]() { return randVal++; });
    nodes.nodes.emplace(makeBuf< dht::Key_t >(0x03), dht::RCNode());
    EXPECT_CALL(context, Nodes()).WillOnce(Return(&nodes));
    ASSERT_TRUE(serviceAddressLookup->GetNextPeer(key, exclude));
  }
}

TEST_F(TestDhtServiceAddressLookup, do_next)
{
  // Concerns:
  // - R != 0
  // - R = 0

  const dht::Key_t key = makeBuf< dht::Key_t >(0x02);

  {
    // R != 0
    EXPECT_CALL(context, LookupIntroSetRecursive(address, txKey, txId, key, r - 1, _));
    ASSERT_NO_THROW(serviceAddressLookup->DoNextRequest(key));
  }

  {
    // R = 0
    serviceAddressLookup->R = 0;
    EXPECT_CALL(context, LookupIntroSetIterative(address, txKey, txId, key, _));
    ASSERT_NO_THROW(serviceAddressLookup->DoNextRequest(key));
  }
}

TEST_F(TestDhtServiceAddressLookup, send_reply)
{
  // Concerns
  // - handle result is set, is called
  // - handle result is not set

  {
    serviceAddressLookup->valuesFound.emplace_back();
    EXPECT_CALL(introsetHandler, call(SizeIs(1)));

    // clang-format off
    EXPECT_CALL(
      context,
      DHTSendTo(
        Eq(txKey.as_array()),
        WhenDynamicCastTo<dht::GotIntroMessage *>(
          AllOf(
            NotNull(),
            Field(&dht::GotIntroMessage::found, SizeIs(1))
          )
        ),
        true
      )
    );
    // clang-format on

    ASSERT_NO_THROW(serviceAddressLookup->SendReply());
  }

  {
    serviceAddressLookup->valuesFound.clear();
    serviceAddressLookup->valuesFound.emplace_back();
    serviceAddressLookup->handleResult =
        decltype(serviceAddressLookup->handleResult)();

    // clang-format off
    EXPECT_CALL(
      context,
      DHTSendTo(
        Eq(txKey.as_array()),
        WhenDynamicCastTo<dht::GotIntroMessage *>(
          AllOf(
            NotNull(),
            Field(&dht::GotIntroMessage::found, SizeIs(1))
          )
        ),
        true
      )
    );
    // clang-format on

    ASSERT_NO_THROW(serviceAddressLookup->SendReply());
  }
}
