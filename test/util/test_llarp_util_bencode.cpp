#include <util/bencode.h>
#include <util/bencode.hpp>

#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

using TestBuffer = std::vector< byte_t >;

template < typename Result >
struct TestReadData
{
  TestBuffer buffer;
  bool rc;
  Result result;
};

using TestReadInt    = TestReadData< uint64_t >;
using TestReadString = TestReadData< std::string >;

template < typename Result >
std::ostream&
operator<<(std::ostream& os, const TestReadData< Result >& d)
{
  os << "buf = [ ";
  for(auto x : d.buffer)
  {
    os << x << " ";
  }

  os << "] rc = ";

  os << std::boolalpha << d.rc << " result = " << d.result;
  return os;
}

struct ReadInt : public ::testing::TestWithParam< TestReadInt >
{
};

TEST_P(ReadInt, readInt)
{
  auto d = GetParam();

  llarp_buffer_t buffer(d.buffer);

  uint64_t result = 0;
  bool rc         = bencode_read_integer(&buffer, &result);

  EXPECT_EQ(rc, d.rc);
  EXPECT_EQ(result, d.result);
}

static constexpr byte_t i     = 'i';
static constexpr byte_t e     = 'e';
static constexpr byte_t zero  = '0';
static constexpr byte_t one   = '1';
static constexpr byte_t two   = '2';
static constexpr byte_t f     = 'f';
static constexpr byte_t z     = 'z';
static constexpr byte_t colon = ':';

static const TestReadInt testReadInt[] = {
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

INSTANTIATE_TEST_CASE_P(TestBencode, ReadInt,
                        ::testing::ValuesIn(testReadInt), );

struct ReadStr : public ::testing::TestWithParam< TestReadString >
{
};

TEST_P(ReadStr, readStr)
{
  auto d = GetParam();

  llarp_buffer_t buffer(d.buffer);

  llarp_buffer_t result;
  bool rc = bencode_read_string(&buffer, &result);

  EXPECT_EQ(rc, d.rc);
  EXPECT_EQ(result.sz, d.result.size());
  EXPECT_EQ(std::string(result.base, result.base + result.sz), d.result);
}

static const TestReadString testReadStr[] = {
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

INSTANTIATE_TEST_CASE_P(TestBencode, ReadStr,
                        ::testing::ValuesIn(testReadStr), );

template < typename Input >
struct TestWriteData
{
  Input input;
  size_t bufferSize;
  bool rc;
  std::string output;
};

using TestWriteByteString = TestWriteData< std::string >;
using TestWriteInt        = TestWriteData< uint64_t >;

struct WriteByteStr : public ::testing::TestWithParam< TestWriteByteString >
{
};

TEST_P(WriteByteStr, writeByte)
{
  auto d = GetParam();

  std::vector< byte_t > backingBuffer(d.bufferSize, 0);
  llarp_buffer_t buffer(backingBuffer);

  bool rc = bencode_write_bytestring(&buffer, d.input.data(), d.input.size());

  ASSERT_EQ(rc, d.rc);
  ASSERT_EQ(std::string(buffer.base, buffer.cur), d.output);
}

static constexpr size_t MAX_1 =
    static_cast< size_t >(std::numeric_limits< int16_t >::max()) + 1;

static const TestWriteByteString testWriteByteString[] = {
    // good cases
    {"abacus", 100, true, "6:abacus"},
    {"  abacus", 100, true, "8:  abacus"},
    {"", 100, true, "0:"},
    {std::string("\0\0\0", 3), 100, true, std::string("3:\0\0\0", 5)},
    {std::string(MAX_1, 'a'), MAX_1 + 100, true,
     std::to_string(MAX_1) + std::string(":") + std::string(MAX_1, 'a')},
    // bad cases
    {"a", 1, false, ""},
};

INSTANTIATE_TEST_CASE_P(TestBencode, WriteByteStr,
                        ::testing::ValuesIn(testWriteByteString), );

struct WriteInt : public ::testing::TestWithParam< TestWriteInt >
{
};

TEST_P(WriteInt, writeInt)
{
  auto d = GetParam();

  std::vector< byte_t > backingBuffer(d.bufferSize, 0);
  llarp_buffer_t buffer(backingBuffer);

  bool rc = bencode_write_uint64(&buffer, d.input);

  ASSERT_EQ(rc, d.rc);
  ASSERT_EQ(std::string(buffer.base, buffer.cur), d.output);
}

static const TestWriteInt testWriteInt[] = {
    // Good cases
    {0, 100, true, "i0e"},
    {1234, 100, true, "i1234e"},
    {uint64_t(-1), 100, true, "i18446744073709551615e"},
    // Bad cases
    {1234567, 3, false, ""},
};

INSTANTIATE_TEST_CASE_P(TestBencode, WriteInt,
                        ::testing::ValuesIn(testWriteInt), );

struct WriteIntValues : public ::testing::TestWithParam< uint64_t >
{
};

TEST_P(WriteIntValues, anyvalue)
{
  // test we can encode any uint64_t into a buffer.
  uint64_t val = GetParam();

  std::vector< byte_t > backingBuffer(100, 0);

  {
    llarp_buffer_t buffer(backingBuffer);

    bool rc = bencode_write_uint64(&buffer, val);
    ASSERT_TRUE(rc);
  }

  {
    uint64_t result = 0;
    llarp_buffer_t buffer(backingBuffer);
    bool rc = bencode_read_integer(&buffer, &result);
    ASSERT_TRUE(rc);
    ASSERT_EQ(result, val);
  }
}

INSTANTIATE_TEST_CASE_P(
    TestBencode, WriteIntValues,
    ::testing::Values(std::numeric_limits< uint64_t >::min(),
                      std::numeric_limits< uint64_t >::max(),
                      std::numeric_limits< uint64_t >::max() / 2,
                      std::numeric_limits< uint64_t >::max() / 3), );

TEST(TestBencode, good_uint64_entry)
{
  std::vector< byte_t > backingBuffer(100, 0);
  llarp_buffer_t buffer(backingBuffer);

  ASSERT_TRUE(bencode_write_uint64_entry(&buffer, "v", 1, 0));

  ASSERT_EQ(std::string(buffer.base, buffer.cur), "1:vi0e");
}

TEST(TestBencode, bad_uint64_entry)
{
  std::vector< byte_t > otherBuffer(1, 0);
  llarp_buffer_t buffer(otherBuffer);

  ASSERT_FALSE(bencode_write_uint64_entry(&buffer, "v", 1, 0));
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
  std::vector< ValueData > list;
  size_t bufferSize;
  std::string result;
};

struct ListTest : public ::testing::TestWithParam< ListTestData >
{
};

TEST_P(ListTest, list)
{
  auto d = GetParam();

  std::vector< byte_t > backingBuffer(d.bufferSize, 0);
  llarp_buffer_t buffer(backingBuffer);

  ASSERT_TRUE(bencode_start_list(&buffer));

  for(const auto& x : d.list)
  {
    if(x.isString)
    {
      ASSERT_TRUE(bencode_write_bytestring(&buffer, x.theString.data(),
                                           x.theString.size()));
    }
    else
    {
      ASSERT_TRUE(bencode_write_uint64(&buffer, x.theInt));
    }
  }

  ASSERT_TRUE(bencode_end(&buffer));

  ASSERT_EQ(std::string(buffer.base, buffer.cur), d.result);
}

ListTestData listTestData[] = {
    {{}, 100, "le"},
    {{{"", 0, true}}, 100, "l0:e"},
    {{{"", 0, false}}, 100, "li0ee"},
    {{{"", 0, false}, {"", 0, true}}, 100, "li0e0:e"},
    {{{"", 123, false}, {"abc", 0, true}}, 100, "li123e3:abce"},
    {{{"", 123, false}, {"abc", 0, true}, {"abc", 0, true}},
     100,
     "li123e3:abc3:abce"},
};

INSTANTIATE_TEST_CASE_P(TestBencode, ListTest,
                        ::testing::ValuesIn(listTestData), );

struct DictTestData
{
  std::vector< std::pair< char, ValueData > > list;
  size_t bufferSize;
  std::string result;
};

struct DictTest : public ::testing::TestWithParam< DictTestData >
{
};

TEST_P(DictTest, dict)
{
  auto d = GetParam();

  std::vector< byte_t > backingBuffer(d.bufferSize, 0);
  llarp_buffer_t buffer(backingBuffer);

  ASSERT_TRUE(bencode_start_dict(&buffer));

  for(const auto& x : d.list)
  {
    ASSERT_TRUE(bencode_write_bytestring(&buffer, &x.first, 1));
    if(x.second.isString)
    {
      ASSERT_TRUE(bencode_write_bytestring(&buffer, x.second.theString.data(),
                                           x.second.theString.size()));
    }
    else
    {
      ASSERT_TRUE(bencode_write_uint64(&buffer, x.second.theInt));
    }
  }

  ASSERT_TRUE(bencode_end(&buffer));

  ASSERT_EQ(std::string(buffer.base, buffer.cur), d.result);
}

DictTestData dictTestData[] = {
    {{}, 100, "de"},
    {{{'a', {"", 0, true}}}, 100, "d1:a0:e"},
    {{{'b', {"", 0, false}}}, 100, "d1:bi0ee"},
    {{{'c', {"", 0, false}}, {'d', {"", 0, true}}}, 100, "d1:ci0e1:d0:e"},
    {{{'e', {"", 123, false}}, {'f', {"abc", 0, true}}},
     100,
     "d1:ei123e1:f3:abce"},
    {{{'a', {"", 123, false}},
      {'b', {"abc", 0, true}},
      {'c', {"abc", 0, true}}},
     100,
     "d1:ai123e1:b3:abc1:c3:abce"},
};

INSTANTIATE_TEST_CASE_P(TestBencode, DictTest,
                        ::testing::ValuesIn(dictTestData), );

struct ReadData
{
  std::string input;
  std::vector< std::string > output;
};

struct DictReadTest : public ::testing::TestWithParam< ReadData >
{
};

TEST_P(DictReadTest, readtest)
{
  auto d = GetParam();

  byte_t* input =
      const_cast< byte_t* >(reinterpret_cast< const byte_t* >(d.input.data()));

  llarp_buffer_t buffer(input, input, d.input.size());

  std::vector< std::string > result;

  ASSERT_TRUE(llarp::bencode_read_dict(
      [&](llarp_buffer_t*, llarp_buffer_t* key) {
        if(key)
        {
          result.emplace_back(key->base, key->base + key->sz);
        }
        return true;
      },
      &buffer));
  ASSERT_EQ(result, d.output);
}

ReadData dictReadData[] = {{"de", {}},
                           {"d1:a0:e", {"a", ""}},
                           {"d1:be", {"b"}},
                           {"d1:b2:23e", {"b", "23"}}};

INSTANTIATE_TEST_CASE_P(TestBencode, DictReadTest,
                        ::testing::ValuesIn(dictReadData), );

struct ListReadTest : public ::testing::TestWithParam< ReadData >
{
};

TEST_P(ListReadTest, readtest)
{
  auto d = GetParam();

  byte_t* input =
      const_cast< byte_t* >(reinterpret_cast< const byte_t* >(d.input.data()));

  llarp_buffer_t buffer(input, input, d.input.size());

  std::vector< std::string > result;

  ASSERT_TRUE(llarp::bencode_read_list(
      [&](llarp_buffer_t* b, bool cont) {
        if(cont)
        {
          llarp_buffer_t tmp;
          bencode_read_string(b, &tmp);
          result.emplace_back(tmp.base, tmp.base + tmp.sz);
        }
        return true;
      },
      &buffer));
  ASSERT_EQ(result, d.output);
}

ReadData listReadData[] = {
    {"le", {}}, {"l1:ae", {"a"}}, {"l1:be", {"b"}}, {"l1:b2:23e", {"b", "23"}}};

INSTANTIATE_TEST_CASE_P(TestBencode, ListReadTest,
                        ::testing::ValuesIn(listReadData), );

TEST(TestBencode, ReadDictEmptyBuffer)
{
  llarp_buffer_t buf((byte_t*)nullptr, 0);
  ASSERT_FALSE(llarp::bencode_read_dict(
      [](llarp_buffer_t*, llarp_buffer_t*) { return true; }, &buf));
}
