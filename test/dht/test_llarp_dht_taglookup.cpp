#include <dht/taglookup.hpp>

#include <crypto/mock_crypto.hpp>
#include <dht/mock_context.hpp>
#include <dht/messages/gotintro.hpp>
#include <llarp_test.hpp>
#include <service/intro_set.hpp>
#include <test_util.hpp>

#include <gtest/gtest.h>

using namespace llarp;
using namespace ::testing;

using test::makeBuf;

static constexpr uint64_t EXPIRY = 1548503831ull;

struct TestDhtTagLookup : public test::LlarpTest<>
{
  dht::Key_t txKey;
  uint64_t txId;
  dht::TXOwner txOwner;
  service::Tag tag;

  test::MockContext context;
  uint64_t r;
  dht::TagLookup tagLookup;

  TestDhtTagLookup()
      : txKey(makeBuf< dht::Key_t >(0x01))
      , txId(2)
      , txOwner(txKey, txId)
      , tag(makeBuf< service::Tag >(0x03))
      , r(4)
      , tagLookup(txOwner, tag, &context, r)
  {
  }
};

TEST_F(TestDhtTagLookup, validate)
{
  // Concerns
  // - introset fails to verify
  // - introset topic is not the target
  // - happy path

  {
    service::IntroSet introset;
    EXPECT_CALL(context, Now()).WillOnce(Return(EXPIRY));
    EXPECT_CALL(m_crypto, verify(_, _, _)).WillOnce(Return(false));

    ASSERT_FALSE(tagLookup.Validate(introset));
  }

  {
    service::IntroSet introset;
    // Set topic to be different to the current tag
    introset.topic = makeBuf< service::Tag >(0x02);

    // Fiddle with the introset so we pass the Verify call
    introset.I.emplace_back();
    introset.I.front().expiresAt =
        EXPIRY + service::MAX_INTROSET_TIME_DELTA + 1;

    // Set expectations
    EXPECT_CALL(context, Now()).WillOnce(Return(EXPIRY));
    EXPECT_CALL(m_crypto, verify(_, _, _)).WillOnce(Return(true));

    ASSERT_FALSE(tagLookup.Validate(introset));
  }

  {
    service::IntroSet introset;
    // Set topic to be equal to the current tag
    introset.topic = tag;

    // Fiddle with the introset so we pass the Verify call
    introset.I.emplace_back();
    introset.I.front().expiresAt =
        EXPIRY + service::MAX_INTROSET_TIME_DELTA + 1;

    // Set expectations
    EXPECT_CALL(context, Now()).WillOnce(Return(EXPIRY));
    EXPECT_CALL(m_crypto, verify(_, _, _)).WillOnce(Return(true));

    ASSERT_TRUE(tagLookup.Validate(introset));
  }
}

TEST_F(TestDhtTagLookup, start)
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

  ASSERT_NO_THROW(tagLookup.Start(txOwner));
}

TEST_F(TestDhtTagLookup, get_next_peer)
{
  dht::Key_t key = makeBuf< dht::Key_t >(0x02);
  std::set< dht::Key_t > exclude;
  ASSERT_FALSE(tagLookup.GetNextPeer(key, exclude));
}

TEST_F(TestDhtTagLookup, do_next)
{
  const dht::Key_t key = makeBuf< dht::Key_t >(0x02);
  ASSERT_NO_THROW(tagLookup.DoNextRequest(key));
}

TEST_F(TestDhtTagLookup, send_reply)
{
  // Concerns
  // - empty values found
  // - when found.size < 2
  //   - FindRandomIntroSetsWithTagExcluding returns empty
  //   - FindRandomIntroSetsWithTagExcluding result are added to call
  // - DHTSendTo called with correct params

  {
    tagLookup.valuesFound.clear();
    // clang-format off
    EXPECT_CALL(context, FindRandomIntroSetsWithTagExcluding(tag, _, IsEmpty()))
      .WillOnce(Return(std::set< service::IntroSet >()));

    EXPECT_CALL(
      context,
      DHTSendTo(
        Eq(txKey.as_array()),
        WhenDynamicCastTo<dht::GotIntroMessage *>(
          AllOf(
            NotNull(),
            Field(&dht::GotIntroMessage::found, IsEmpty())
          )
        ),
        true
      )
    );
    // clang-format on
    ASSERT_NO_THROW(tagLookup.SendReply());
  }

  {
    tagLookup.valuesFound.clear();
    std::set< service::IntroSet > results;
    results.emplace();
    // clang-format off
    EXPECT_CALL(context, FindRandomIntroSetsWithTagExcluding(tag, _, IsEmpty()))
      .WillOnce(Return(results));

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
    ASSERT_NO_THROW(tagLookup.SendReply());
  }

  {
    // clang-format off
    tagLookup.valuesFound.clear();
    tagLookup.valuesFound.emplace_back();
    EXPECT_CALL(context, FindRandomIntroSetsWithTagExcluding(tag, _, SizeIs(1)))
      .WillOnce(Return(std::set< service::IntroSet >()));

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
    ASSERT_NO_THROW(tagLookup.SendReply());
  }

  {
    tagLookup.valuesFound.clear();
    tagLookup.valuesFound.emplace_back();
    tagLookup.valuesFound.back().T           = 1;
    tagLookup.valuesFound.back().A.vanity[0] = 1;
    tagLookup.valuesFound.back().A.UpdateAddr();
    tagLookup.valuesFound.emplace_back();
    tagLookup.valuesFound.back().T           = 2;
    tagLookup.valuesFound.back().A.vanity[0] = 2;
    tagLookup.valuesFound.back().A.UpdateAddr();
    // clang-format off
    EXPECT_CALL(context, FindRandomIntroSetsWithTagExcluding(_, _, _)).Times(0);

    EXPECT_CALL(
      context,
      DHTSendTo(
        Eq(txKey.as_array()),
        WhenDynamicCastTo<dht::GotIntroMessage *>(
          AllOf(
            NotNull(),
            Field(&dht::GotIntroMessage::found, SizeIs(2))
          )
        ),
        true
      )
    );
    // clang-format on
    ASSERT_NO_THROW(tagLookup.SendReply());
  }
}
