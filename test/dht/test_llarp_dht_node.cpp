#include <dht/node.hpp>

#include <test_util.hpp>

#include <catch2/catch.hpp>

using namespace llarp;

using test::makeBuf;

TEST_CASE("dht::RCNode construct", "[dht]")
{
  dht::RCNode node;
  REQUIRE(node.ID.IsZero());

  node.ID.Fill(0xCA);
  node.rc.last_updated = 101s;

  dht::RCNode other{node};
  REQUIRE(node.ID == other.ID);
  REQUIRE(node.rc == other.rc);

  RouterContact contact;
  contact.pubkey.Randomize();

  dht::RCNode fromContact{contact};
  REQUIRE(fromContact.ID.as_array() == contact.pubkey.as_array());
}

TEST_CASE("dht::RCNode <", "[dht]")
{
  dht::RCNode one;
  dht::RCNode two;
  dht::RCNode three;
  dht::RCNode eqThree;

  one.rc.last_updated = 1s;
  two.rc.last_updated = 2s;
  three.rc.last_updated = 3s;
  eqThree.rc.last_updated = 3s;

  // LT cases
  REQUIRE(one < two);
  REQUIRE(one < three);
  REQUIRE(one < eqThree);
  REQUIRE(two < three);
  REQUIRE(two < eqThree);

  // !LT cases
  REQUIRE(!(one < one));
  REQUIRE(!(two < one));
  REQUIRE(!(two < two));
  REQUIRE(!(three < one));
  REQUIRE(!(three < two));
  REQUIRE(!(three < three));
  REQUIRE(!(three < eqThree));
}

TEST_CASE("dht::ISNode construct", "[dht]")
{
  dht::ISNode node;
  REQUIRE(node.ID.IsZero());

  node.ID.Fill(0xCA);
  node.introset.derivedSigningKey.Fill(0xDB);

  dht::ISNode other{node};
  REQUIRE(node.ID == other.ID);
  REQUIRE(node.introset == other.introset);

  service::EncryptedIntroSet introSet;
  introSet.derivedSigningKey.Randomize();

  dht::ISNode fromIntro{introSet};

  REQUIRE(fromIntro.ID.as_array() == introSet.derivedSigningKey);
}

TEST_CASE("dht::ISNode <", "[dht]")
{
  dht::ISNode one;
  dht::ISNode two;
  dht::ISNode three;
  dht::ISNode eqThree;

  one.introset.signedAt = 1s;
  two.introset.signedAt = 2s;
  three.introset.signedAt = 3s;
  eqThree.introset.signedAt = 3s;

  // LT cases
  REQUIRE(one < two);
  REQUIRE(one < three);
  REQUIRE(one < eqThree);
  REQUIRE(two < three);
  REQUIRE(two < eqThree);

  // !LT cases
  REQUIRE(!(one < one));
  REQUIRE(!(two < one));
  REQUIRE(!(two < two));
  REQUIRE(!(three < one));
  REQUIRE(!(three < two));
  REQUIRE(!(three < three));
  REQUIRE(!(three < eqThree));
}
