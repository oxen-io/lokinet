#include <dht/kademlia.hpp>

#include <catch2/catch.hpp>

using llarp::dht::Key_t;

using Array = std::array<byte_t, Key_t::SIZE>;

struct XorMetricData
{
  Array us;
  Array left;
  Array right;
  bool result;

  XorMetricData(const Array& u, const Array& l, const Array& r, bool res)
      : us(u), left(l), right(r), result(res)
  {}
};

std::ostream&
operator<<(std::ostream& stream, const XorMetricData& x)
{
  stream << int(x.us[0]) << " " << int(x.left[0]) << " " << int(x.right[0]) << " " << std::boolalpha
         << x.result;

  return stream;
}
std::vector<XorMetricData>
makeData()
{
  std::vector<XorMetricData> result;

  Array zero;
  zero.fill(0);
  Array one;
  one.fill(1);
  Array two;
  two.fill(2);
  Array three;
  three.fill(3);

  result.emplace_back(zero, zero, zero, false);
  result.emplace_back(zero, zero, one, true);
  result.emplace_back(zero, zero, two, true);
  result.emplace_back(zero, one, zero, false);
  result.emplace_back(zero, one, one, false);
  result.emplace_back(zero, one, two, true);
  result.emplace_back(zero, two, zero, false);
  result.emplace_back(zero, two, one, false);
  result.emplace_back(zero, two, two, false);
  result.emplace_back(one, zero, zero, false);
  result.emplace_back(one, zero, one, false);
  result.emplace_back(one, zero, two, true);
  result.emplace_back(one, one, zero, true);
  result.emplace_back(one, one, one, false);
  result.emplace_back(one, one, two, true);
  result.emplace_back(one, two, zero, false);
  result.emplace_back(one, two, one, false);
  result.emplace_back(one, two, two, false);
  result.emplace_back(two, zero, zero, false);
  result.emplace_back(two, zero, one, true);
  result.emplace_back(two, zero, two, false);
  result.emplace_back(two, one, zero, false);
  result.emplace_back(two, one, one, false);
  result.emplace_back(two, one, two, false);
  result.emplace_back(two, two, zero, true);
  result.emplace_back(two, two, one, true);
  result.emplace_back(two, two, two, false);

  return result;
}

TEST_CASE("XorMetric", "[dht]")
{
  auto d = GENERATE(from_range(makeData()));
  REQUIRE(llarp::dht::XorMetric{Key_t{d.us}}(Key_t{d.left}, Key_t{d.right}) == d.result);
}
