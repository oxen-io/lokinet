#include <dht/tx.hpp>
#include <service/tag.hpp>
#include <test_util.hpp>

#include <catch2/catch.hpp>
#include <gmock/gmock.h>

using namespace llarp;
using namespace ::testing;

using llarp::test::makeBuf;

using Val_t = llarp::service::Tag;

// Mock implementation of TX.
struct TestTx : public dht::TX<dht::Key_t, Val_t>
{
  TestTx(const dht::TXOwner& asker, const dht::Key_t& k, dht::AbstractContext* p)
      : dht::TX<dht::Key_t, Val_t>(asker, k, p)
  {}

  MOCK_CONST_METHOD1(Validate, bool(const Val_t&));

  MOCK_METHOD1(Start, void(const dht::TXOwner&));

  MOCK_METHOD0(SendReply, void());
};

struct TestDhtTx
{
  dht::TXOwner asker;
  dht::Key_t m_key;
  TestTx tx;

  TestDhtTx() : tx(asker, m_key, nullptr)
  {}

  ~TestDhtTx()
  {
    CHECK(Mock::VerifyAndClearExpectations(&tx));
  }
};

// TODO: sections?
TEST_CASE_METHOD(TestDhtTx, "on_found", "[dht]")
{
  // Concerns
  // - Validate returns true
  // - Repeated call on success
  // - Validate returns false
  // - Repeated call on failure
  // - Repeated call on success after failure

  const auto key = makeBuf<dht::Key_t>(0x00);
  Val_t val("good value");

  // Validate returns true
  {
    EXPECT_CALL(tx, Validate(val)).WillOnce(Return(true));

    tx.OnFound(key, val);

    REQUIRE(tx.peersAsked.count(key) > 0);
    REQUIRE_THAT(tx.valuesFound, Catch::VectorContains(val));
  }

  // Repeated call on success
  {
    EXPECT_CALL(tx, Validate(val)).WillOnce(Return(true));
    tx.OnFound(key, val);
    REQUIRE(tx.peersAsked.count(key) > 0);
    REQUIRE_THAT(tx.valuesFound, Catch::VectorContains(val));
  }

  const auto key1 = makeBuf<dht::Key_t>(0x01);
  Val_t badVal("bad value");

  // Validate returns false
  {
    EXPECT_CALL(tx, Validate(badVal)).WillOnce(Return(false));

    tx.OnFound(key1, badVal);

    REQUIRE(tx.peersAsked.count(key1) > 0);
    REQUIRE_THAT(tx.valuesFound, !Catch::VectorContains(badVal));
  }

  // Repeated call on failure
  {
    EXPECT_CALL(tx, Validate(badVal)).WillOnce(Return(false));

    tx.OnFound(key1, badVal);

    REQUIRE(tx.peersAsked.count(key1) > 0);
    REQUIRE_THAT(tx.valuesFound, !Catch::VectorContains(badVal));
  }

  // Repeated call on success after failure
  {
    EXPECT_CALL(tx, Validate(badVal)).WillOnce(Return(true));

    tx.OnFound(key1, badVal);

    REQUIRE(tx.peersAsked.count(key1) > 0);
    REQUIRE_THAT(tx.valuesFound, Catch::VectorContains(badVal));
  }
}
