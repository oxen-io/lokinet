#include <util/bencode.h>
#include <util/bencode.hpp>

#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch.hpp>

using TestBuffer = std::vector<byte_t>;

template <typename Result>
struct TestReadData
{
  TestBuffer buffer;
  bool rc;
  Result result;
};

using TestReadInt = TestReadData<uint64_t>;
using TestReadString = TestReadData<std::string>;

template <typename Result>
std::ostream&
operator<<(std::ostream& os, const TestReadData<Result>& d)
{
  os << "buf = [ ";
  for (auto x : d.buffer)
  {
    os << x << " ";
  }

  os << "] rc = ";

  os << std::boolalpha << d.rc << " result = " << d.result;
  return os;
}

static constexpr byte_t i = 'i';
static constexpr byte_t e = 'e';
static constexpr byte_t zero = '0';
static constexpr byte_t one = '1';
static constexpr byte_t two = '2';
static constexpr byte_t f = 'f';
static constexpr byte_t z = 'z';
static constexpr byte_t colon = ':';

std::vector<TestReadInt> testReadInt{
    // good cases
    {{i, 0, e}, true, 0},
    {{i, zero, e}, true, 0},
    {{i, one, e}, true, 1},
    {{i, two, e}, true, 2},
    {{i, two, e, e, e}, true, 2},
    {{i, one, one, one, two, e}, true, 1112},
    {{i, f, e}, true, 0},
    {{i, z, e}, true, 0},
    {{i, one, two, e, one, one}, true, 12},
    // failure cases
    {{i, e}, false, 0},
    {{e}, false, 0},
    {{z}, false, 0},
};

TEST_CASE("Read int", "[bencode]")
{
  auto d = GENERATE(from_range(testReadInt));

  llarp_buffer_t buffer(d.buffer);

  uint64_t result = 0;
  bool rc = bencode_read_integer(&buffer, &result);

  CHECK(rc == d.rc);
  CHECK(result == d.result);
}

std::vector<TestReadString> testReadStr{
    // good cases
    {{one, colon, 'a'}, true, "a"},
    {{one, colon, 'b'}, true, "b"},
    {{two, colon, f, z}, true, "fz"},
    {{two, colon, f, z, f, f}, true, "fz"},
    {{zero, colon}, true, ""},
    // failure cases
    {{two, colon, f}, false, ""},
    {{two, f}, false, ""},
    {{'-', one, colon, f}, false, ""},
    {{f}, false, ""},
    {{one, f, colon}, false, ""},
    {{colon}, false, ""},
    {{colon, colon}, false, ""},
};

TEST_CASE("Read str", "[bencode]")
{
  auto d = GENERATE(from_range(testReadStr));

  llarp_buffer_t buffer(d.buffer);

  llarp_buffer_t result;
  bool rc = bencode_read_string(&buffer, &result);

  CHECK(rc == d.rc);
  CHECK(result.sz == d.result.size());
  CHECK(std::string(result.base, result.base + result.sz) == d.result);
}

template <typename Input>
struct TestWriteData
{
  Input input;
  size_t bufferSize;
  bool rc;
  std::string output;
};

using TestWriteByteString = TestWriteData<std::string>;
using TestWriteInt = TestWriteData<uint64_t>;

static constexpr size_t MAX_1 = static_cast<size_t>(std::numeric_limits<int16_t>::max()) + 1;

std::vector<TestWriteByteString> testWriteByteString{
    // good cases
    {"abacus", 100, true, "6:abacus"},
    {"  abacus", 100, true, "8:  abacus"},
    {"", 100, true, "0:"},
    {std::string("\0\0\0", 3), 100, true, std::string("3:\0\0\0", 5)},
    {std::string(MAX_1, 'a'),
     MAX_1 + 100,
     true,
     std::to_string(MAX_1) + std::string(":") + std::string(MAX_1, 'a')},
    // bad cases
    {"a", 1, false, ""},
};

TEST_CASE("Write byte str", "[bencode]")
{
  auto d = GENERATE(from_range(testWriteByteString));

  std::vector<byte_t> backingBuffer(d.bufferSize, 0);
  llarp_buffer_t buffer(backingBuffer);

  bool rc = bencode_write_bytestring(&buffer, d.input.data(), d.input.size());

  REQUIRE(rc == d.rc);
  REQUIRE(std::string(buffer.base, buffer.cur) == d.output);
}

std::vector<TestWriteInt> testWriteInt{
    // Good cases
    {0, 100, true, "i0e"},
    {1234, 100, true, "i1234e"},
    {uint64_t(-1), 100, true, "i18446744073709551615e"},
    // Bad cases
    {1234567, 3, false, ""},
};

TEST_CASE("Write int", "[bencode]")
{
  auto d = GENERATE(from_range(testWriteInt));

  std::vector<byte_t> backingBuffer(d.bufferSize, 0);
  llarp_buffer_t buffer(backingBuffer);

  bool rc = bencode_write_uint64(&buffer, d.input);

  REQUIRE(rc == d.rc);
  REQUIRE(std::string(buffer.base, buffer.cur) == d.output);
}

TEST_CASE("Write int values", "[bencode]")
{
  // test we can encode any uint64_t into a buffer.
  uint64_t val = GENERATE(
      std::numeric_limits<uint64_t>::min(),
      std::numeric_limits<uint64_t>::max(),
      std::numeric_limits<uint64_t>::max() / 2,
      std::numeric_limits<uint64_t>::max() / 3);

  std::vector<byte_t> backingBuffer(100, 0);

  {
    llarp_buffer_t buffer(backingBuffer);

    bool rc = bencode_write_uint64(&buffer, val);
    REQUIRE(rc);
  }

  {
    uint64_t result = 0;
    llarp_buffer_t buffer(backingBuffer);
    bool rc = bencode_read_integer(&buffer, &result);
    REQUIRE(rc);
    REQUIRE(result == val);
  }
}

TEST_CASE("Bencode: good uint64 entry", "[bencode]")
{
  std::vector<byte_t> backingBuffer(100, 0);
  llarp_buffer_t buffer(backingBuffer);

  REQUIRE(bencode_write_uint64_entry(&buffer, "v", 1, 0));

  REQUIRE(std::string(buffer.base, buffer.cur) == "1:vi0e");
}

TEST_CASE("Bencode: bad uint64 entry", "[bencode]")
{
  std::vector<byte_t> otherBuffer(1, 0);
  llarp_buffer_t buffer(otherBuffer);

  REQUIRE_FALSE(bencode_write_uint64_entry(&buffer, "v", 1, 0));
}

struct ValueData
{
  // Variant-ish
  std::string theString;
  uint64_t theInt;
  bool isString;
};

struct ListTestData
{
  std::vector<ValueData> list;
  size_t bufferSize;
  std::string result;
};

std::vector<ListTestData> listTestData{
    {{}, 100, "le"},
    {{{"", 0, true}}, 100, "l0:e"},
    {{{"", 0, false}}, 100, "li0ee"},
    {{{"", 0, false}, {"", 0, true}}, 100, "li0e0:e"},
    {{{"", 123, false}, {"abc", 0, true}}, 100, "li123e3:abce"},
    {{{"", 123, false}, {"abc", 0, true}, {"abc", 0, true}}, 100, "li123e3:abc3:abce"},
};

TEST_CASE("List test", "[bencode]")
{
  auto d = GENERATE(from_range(listTestData));

  std::vector<byte_t> backingBuffer(d.bufferSize, 0);
  llarp_buffer_t buffer(backingBuffer);

  REQUIRE(bencode_start_list(&buffer));

  for (const auto& x : d.list)
  {
    if (x.isString)
    {
      REQUIRE(bencode_write_bytestring(&buffer, x.theString.data(), x.theString.size()));
    }
    else
    {
      REQUIRE(bencode_write_uint64(&buffer, x.theInt));
    }
  }

  REQUIRE(bencode_end(&buffer));

  REQUIRE(std::string(buffer.base, buffer.cur) == d.result);
}

struct DictTestData
{
  std::vector<std::pair<char, ValueData>> list;
  size_t bufferSize;
  std::string result;
};

std::vector<DictTestData> dictTestData{
    {{}, 100, "de"},
    {{{'a', {"", 0, true}}}, 100, "d1:a0:e"},
    {{{'b', {"", 0, false}}}, 100, "d1:bi0ee"},
    {{{'c', {"", 0, false}}, {'d', {"", 0, true}}}, 100, "d1:ci0e1:d0:e"},
    {{{'e', {"", 123, false}}, {'f', {"abc", 0, true}}}, 100, "d1:ei123e1:f3:abce"},
    {{{'a', {"", 123, false}}, {'b', {"abc", 0, true}}, {'c', {"abc", 0, true}}},
     100,
     "d1:ai123e1:b3:abc1:c3:abce"},
};

TEST_CASE("Dict test", "[bencode]")
{
  auto d = GENERATE(from_range(dictTestData));

  std::vector<byte_t> backingBuffer(d.bufferSize, 0);
  llarp_buffer_t buffer(backingBuffer);

  REQUIRE(bencode_start_dict(&buffer));

  for (const auto& x : d.list)
  {
    REQUIRE(bencode_write_bytestring(&buffer, &x.first, 1));
    if (x.second.isString)
    {
      REQUIRE(
          bencode_write_bytestring(&buffer, x.second.theString.data(), x.second.theString.size()));
    }
    else
    {
      REQUIRE(bencode_write_uint64(&buffer, x.second.theInt));
    }
  }

  REQUIRE(bencode_end(&buffer));

  REQUIRE(std::string(buffer.base, buffer.cur) == d.result);
}

struct ReadData
{
  std::string input;
  std::vector<std::string> output;
};

std::vector<ReadData> dictReadData{
    {"de", {}}, {"d1:a0:e", {"a", ""}}, {"d1:be", {"b"}}, {"d1:b2:23e", {"b", "23"}}};

TEST_CASE("Read dict", "[bencode]")
{
  auto d = GENERATE(from_range(dictReadData));

  byte_t* input = const_cast<byte_t*>(reinterpret_cast<const byte_t*>(d.input.data()));

  llarp_buffer_t buffer(input, input, d.input.size());

  std::vector<std::string> result;

  REQUIRE(llarp::bencode_read_dict(
      [&](llarp_buffer_t*, llarp_buffer_t* key) {
        if (key)
        {
          result.emplace_back(key->base, key->base + key->sz);
        }
        return true;
      },
      &buffer));
  REQUIRE(result == d.output);
}

std::vector<ReadData> listReadData{
    {"le", {}}, {"l1:ae", {"a"}}, {"l1:be", {"b"}}, {"l1:b2:23e", {"b", "23"}}};

TEST_CASE("Read list", "[bencode]")
{
  auto d = GENERATE(from_range(listReadData));

  byte_t* input = const_cast<byte_t*>(reinterpret_cast<const byte_t*>(d.input.data()));

  llarp_buffer_t buffer(input, input, d.input.size());

  std::vector<std::string> result;

  REQUIRE(llarp::bencode_read_list(
      [&](llarp_buffer_t* b, bool cont) {
        if (cont)
        {
          llarp_buffer_t tmp;
          bencode_read_string(b, &tmp);
          result.emplace_back(tmp.base, tmp.base + tmp.sz);
        }
        return true;
      },
      &buffer));
  REQUIRE(result == d.output);
}

TEST_CASE("Read dict to empty buffer", "[bencode]")
{
  llarp_buffer_t buf((byte_t*)nullptr, 0);
  REQUIRE_FALSE(
      llarp::bencode_read_dict([](llarp_buffer_t*, llarp_buffer_t*) { return true; }, &buf));
}
