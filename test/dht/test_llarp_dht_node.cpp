#include <dht/node.hpp>

#include <test_util.hpp>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

using namespace llarp;
using namespace ::testing;

using test::makeBuf;

struct TestDhtRCNode : public ::testing::Test
{
};

TEST_F(TestDhtRCNode, construct)
{
  dht::RCNode node;
  ASSERT_THAT(node.ID, Property(&dht::Key_t::IsZero, true));

  node.ID.Fill(0xCA);
  node.rc.last_updated = 101;

  dht::RCNode other{node};
  ASSERT_EQ(node.ID, other.ID);
  ASSERT_EQ(node.rc, other.rc);

  RouterContact contact;
  contact.pubkey.Randomize();

  dht::RCNode fromContact{contact};
  ASSERT_EQ(fromContact.ID.as_array(), contact.pubkey.as_array());
}

TEST_F(TestDhtRCNode, lt)
{
  dht::RCNode one;
  dht::RCNode two;
  dht::RCNode three;
  dht::RCNode eqThree;

  one.rc.last_updated     = 1;
  two.rc.last_updated     = 2;
  three.rc.last_updated   = 3;
  eqThree.rc.last_updated = 3;

  // LT cases
  ASSERT_THAT(one, Lt(two));
  ASSERT_THAT(one, Lt(three));
  ASSERT_THAT(one, Lt(eqThree));
  ASSERT_THAT(two, Lt(three));
  ASSERT_THAT(two, Lt(eqThree));

  // !LT cases
  ASSERT_THAT(one, Not(Lt(one)));
  ASSERT_THAT(two, Not(Lt(one)));
  ASSERT_THAT(two, Not(Lt(two)));
  ASSERT_THAT(three, Not(Lt(one)));
  ASSERT_THAT(three, Not(Lt(two)));
  ASSERT_THAT(three, Not(Lt(three)));
  ASSERT_THAT(three, Not(Lt(eqThree)));
}

struct TestDhtISNode : public ::testing::Test
{
};

TEST_F(TestDhtISNode, construct)
{
  dht::ISNode node;
  ASSERT_THAT(node.ID, Property(&dht::Key_t::IsZero, true));

  node.ID.Fill(0xCA);
  node.introset.derivedSigningKey.Fill(0xDB);

  dht::ISNode other{node};
  ASSERT_EQ(node.ID, other.ID);
  ASSERT_EQ(node.introset, other.introset);

  service::EncryptedIntroSet introSet;
  introSet.derivedSigningKey.Randomize();

  dht::ISNode fromIntro{introSet};

  ASSERT_EQ(fromIntro.ID.as_array(), introSet.derivedSigningKey);
}

TEST_F(TestDhtISNode, lt)
{
  dht::ISNode one;
  dht::ISNode two;
  dht::ISNode three;
  dht::ISNode eqThree;

  one.introset.signedAt     = 1;
  two.introset.signedAt     = 2;
  three.introset.signedAt   = 3;
  eqThree.introset.signedAt = 3;

  // LT cases
  ASSERT_THAT(one, Lt(two));
  ASSERT_THAT(one, Lt(three));
  ASSERT_THAT(one, Lt(eqThree));
  ASSERT_THAT(two, Lt(three));
  ASSERT_THAT(two, Lt(eqThree));

  // !LT cases
  ASSERT_THAT(one, Not(Lt(one)));
  ASSERT_THAT(two, Not(Lt(one)));
  ASSERT_THAT(two, Not(Lt(two)));
  ASSERT_THAT(three, Not(Lt(one)));
  ASSERT_THAT(three, Not(Lt(two)));
  ASSERT_THAT(three, Not(Lt(three)));
  ASSERT_THAT(three, Not(Lt(eqThree)));
}
