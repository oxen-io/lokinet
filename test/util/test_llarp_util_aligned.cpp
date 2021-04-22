#include <catch2/catch.hpp>

#include <util/aligned.hpp>

#include <iostream>
#include <sstream>
#include <type_traits>
#include <unordered_map>

using TestSizes = std::tuple<
    std::integral_constant<std::size_t, 8>,
    std::integral_constant<std::size_t, 12>,
    std::integral_constant<std::size_t, 16>,
    std::integral_constant<std::size_t, 32>,
    std::integral_constant<std::size_t, 64>,
    std::integral_constant<std::size_t, 77>,
    std::integral_constant<std::size_t, 1024>,
    std::integral_constant<std::size_t, 3333>>;

TEMPLATE_LIST_TEST_CASE("AlignedBuffer", "[AlignedBuffer]", TestSizes)
{
  using Buffer = llarp::AlignedBuffer<TestType::value>;

  Buffer b;
  CHECK(b.IsZero());

  SECTION("Constructor")
  {
    CHECK(b.size() == TestType::value);
  }

  SECTION("CopyConstructor")
  {
    Buffer c = b;
    CHECK(c.IsZero());

    c.Fill(1);
    CHECK_FALSE(c.IsZero());

    Buffer d = c;
    CHECK_FALSE(d.IsZero());
  }

  SECTION("AltConstructors")
  {
    b.Fill(2);

    Buffer c(b.as_array());
    CHECK_FALSE(c.IsZero());

    Buffer d(c.data());
    CHECK_FALSE(d.IsZero());
  }

  SECTION("Assignment")
  {
    Buffer c;
    c = b;
    CHECK(c.IsZero());

    c.Fill(1);
    CHECK_FALSE(c.IsZero());

    Buffer d;
    d = c;
    CHECK_FALSE(d.IsZero());
  }

  SECTION("StreamOut")
  {
    std::stringstream stream;

    stream << b;

    CHECK(stream.str() == std::string(TestType::value * 2, '0'));

    stream.str("");

    b.Fill(255);
    stream << b;

    CHECK(stream.str() == std::string(TestType::value * 2, 'f'));
  }

  SECTION("BitwiseNot")
  {
    Buffer c = ~b;
    CHECK_FALSE(c.IsZero());

    for (auto val : c.as_array())
    {
      CHECK(255 == val);
    }

    Buffer d = ~c;
    CHECK(d.IsZero());
  }

  SECTION("Operators")
  {
    Buffer c = b;
    CHECK(b == c);
    CHECK(b >= c);
    CHECK(b <= c);
    CHECK(c >= b);
    CHECK(c <= b);

    c.Fill(1);
    CHECK(b != c);
    CHECK(b < c);
    CHECK(c > b);
  }

  SECTION("Xor")
  {
    Buffer c;
    b.Fill(255);
    c.Fill(255);
    CHECK_FALSE(b.IsZero());
    CHECK_FALSE(c.IsZero());

    Buffer d = b ^ c;
    // 1 ^ 1 = 0
    CHECK(d.IsZero());
    // Verify unchanged
    CHECK_FALSE(b.IsZero());
    CHECK_FALSE(c.IsZero());

    Buffer e, f;
    e.Fill(255);
    Buffer g = e ^ f;
    // 1 ^ 0 = 1
    CHECK_FALSE(g.IsZero());

    Buffer h, i;
    i.Fill(255);
    Buffer j = h ^ i;
    // 0 ^ 1 = 1
    CHECK_FALSE(j.IsZero());
  }

  SECTION("XorAssign")
  {
    Buffer c;
    b.Fill(255);
    c.Fill(255);
    CHECK_FALSE(b.IsZero());
    CHECK_FALSE(c.IsZero());

    b ^= c;
    CHECK(b.IsZero());
  }

  SECTION("Zero")
  {
    b.Fill(127);
    CHECK_FALSE(b.IsZero());

    b.Zero();
    CHECK(b.IsZero());
  }

  SECTION("TestHash")
  {
    using Map_t = std::unordered_map<Buffer, int>;

    Buffer k, other_k;
    k.Randomize();
    other_k.Randomize();
    Map_t m;
    CHECK(m.empty());
    CHECK(m.emplace(k, 1).second);
    CHECK(m.find(k) != m.end());
    CHECK(m[k] == 1);
    CHECK_FALSE(m.find(other_k) != m.end());
    CHECK(m.size() == 1);
    Buffer k_copy = k;
    CHECK_FALSE(m.emplace(k_copy, 2).second);
    CHECK_FALSE(m[k_copy] == 2);
    CHECK(m[k_copy] == 1);
  }
}
