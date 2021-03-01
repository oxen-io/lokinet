#include <catch2/catch.hpp>

#include <dht/key.hpp>

using namespace llarp;

using Array = std::array<byte_t, dht::Key_t::SIZE>;

static constexpr Array emptyArray{{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};

static constexpr Array fullArray{{0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                                  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                                  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff}};

static constexpr Array seqArray{{0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A,
                                 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15,
                                 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F}};

std::vector<Array> data{emptyArray, fullArray, seqArray};

TEST_CASE("DHT key constructor", "[dht]")
{
  auto d = GENERATE(from_range(data));

  dht::Key_t a(d);
  dht::Key_t b(d.data());
  dht::Key_t c;

  REQUIRE(a == b);

  if (a.IsZero())
  {
    REQUIRE(a == c);
  }
  else
  {
    REQUIRE(a != c);
  }
}

TEST_CASE("DHT key ==", "[dht]")
{
  REQUIRE(dht::Key_t(emptyArray) == dht::Key_t(emptyArray));
  REQUIRE(dht::Key_t(fullArray) == dht::Key_t(fullArray));
  REQUIRE(dht::Key_t(seqArray) == dht::Key_t(seqArray));
}

TEST_CASE("DHT key !=", "[dht]")
{
  REQUIRE(dht::Key_t(emptyArray) != dht::Key_t(fullArray));
  REQUIRE(dht::Key_t(emptyArray) != dht::Key_t(seqArray));
  REQUIRE(dht::Key_t(fullArray) != dht::Key_t(seqArray));
}

TEST_CASE("DHT key <", "[dht]")
{
  REQUIRE(dht::Key_t(emptyArray) < dht::Key_t(fullArray));
  REQUIRE(dht::Key_t(emptyArray) < dht::Key_t(seqArray));
  REQUIRE(dht::Key_t(seqArray) < dht::Key_t(fullArray));
}

TEST_CASE("DHT key >", "[dht]")
{
  REQUIRE(dht::Key_t(fullArray) > dht::Key_t(emptyArray));
  REQUIRE(dht::Key_t(seqArray) > dht::Key_t(emptyArray));
  REQUIRE(dht::Key_t(fullArray) > dht::Key_t(seqArray));
}

TEST_CASE("DHT key ^", "[dht]")
{
  REQUIRE(dht::Key_t(emptyArray) == (dht::Key_t(emptyArray) ^ dht::Key_t(emptyArray)));

  REQUIRE(dht::Key_t(seqArray) == (dht::Key_t(emptyArray) ^ dht::Key_t(seqArray)));

  REQUIRE(dht::Key_t(fullArray) == (dht::Key_t(emptyArray) ^ dht::Key_t(fullArray)));

  REQUIRE(dht::Key_t(emptyArray) == (dht::Key_t(fullArray) ^ dht::Key_t(fullArray)));

  REQUIRE(dht::Key_t(emptyArray) == (dht::Key_t(seqArray) ^ dht::Key_t(seqArray)));

  Array xorResult;
  std::iota(xorResult.rbegin(), xorResult.rend(), 0xE0);
  REQUIRE(dht::Key_t(xorResult) == (dht::Key_t(seqArray) ^ dht::Key_t(fullArray)));
}

TEST_CASE("DHT key: test bucket operators", "[dht]")
{
  dht::Key_t zero;
  dht::Key_t one;
  dht::Key_t three;

  zero.Zero();
  one.Fill(1);
  three.Fill(3);
  REQUIRE(zero < one);
  REQUIRE(zero < three);
  REQUIRE_FALSE(zero > one);
  REQUIRE_FALSE(zero > three);
  REQUIRE(zero != three);
  REQUIRE_FALSE(zero == three);
  REQUIRE((zero ^ one) == one);
  REQUIRE(one < three);
  REQUIRE(three > one);
  REQUIRE(one != three);
  REQUIRE_FALSE(one == three);
  REQUIRE((one ^ three) == (three ^ one));
}
