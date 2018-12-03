#include <llarp/dns/dns.hpp>
#include <gtest/gtest.h>
#include <algorithm>

struct DNSLibTest : public ::testing::Test
{
  byte_t mem[1500] = {0};
  llarp_buffer_t buf;

  void
  SetUp()
  {
    buf = llarp::StackBuffer< decltype(mem) >(mem);
  }

  void
  TearDown()
  {
    Rewind();
  }

  void
  Rewind()
  {
    buf.cur = buf.base;
  }
};

TEST_F(DNSLibTest, TestEncodeDecode_RData)
{
  static constexpr size_t rdatasize = 32;
  llarp::dns::RR_RData_t rdata(rdatasize);
  std::fill(rdata.begin(), rdata.end(), 'a');

  llarp::dns::RR_RData_t other_rdata;

  ASSERT_TRUE(llarp::dns::EncodeRData(&buf, rdata));
  ASSERT_TRUE(buf.cur - buf.base == rdatasize + sizeof(uint16_t));
  Rewind();
  ASSERT_TRUE(llarp::dns::DecodeRData(&buf, other_rdata));
  ASSERT_TRUE(rdata == other_rdata);
};

TEST_F(DNSLibTest, TestEncode_huint16)
{
  llarp::huint16_t i = {0x1122};
  ASSERT_TRUE(llarp::dns::EncodeInt(&buf, i));
  ASSERT_TRUE(buf.cur - buf.base == sizeof(uint16_t));
  ASSERT_TRUE(buf.base[0] == 0x11);
  ASSERT_TRUE(buf.base[1] == 0x22);
};

TEST_F(DNSLibTest, TestEncode_huint32)
{
  llarp::huint32_t i = {0x11223344};
  ASSERT_TRUE(llarp::dns::EncodeInt(&buf, i));
  ASSERT_TRUE(buf.cur - buf.base == sizeof(uint32_t));
  ASSERT_TRUE(buf.base[0] == 0x11);
  ASSERT_TRUE(buf.base[1] == 0x22);
  ASSERT_TRUE(buf.base[2] == 0x33);
  ASSERT_TRUE(buf.base[3] == 0x44);
};

TEST_F(DNSLibTest, TestDecode_huint16)
{
  llarp::huint16_t i = {0};
  buf.base[0]        = 0x11;
  buf.base[1]        = 0x22;
  ASSERT_TRUE(llarp::dns::DecodeInt(&buf, i));
  ASSERT_TRUE(buf.cur - buf.base == sizeof(uint16_t));
  ASSERT_TRUE(i.h == 0x1122);
};

TEST_F(DNSLibTest, TestDecode_huint32)
{
  llarp::huint32_t i = {0};
  buf.base[0]        = 0x11;
  buf.base[1]        = 0x22;
  buf.base[2]        = 0x33;
  buf.base[3]        = 0x44;
  ASSERT_TRUE(llarp::dns::DecodeInt(&buf, i));
  ASSERT_TRUE(buf.cur - buf.base == sizeof(uint32_t));
  ASSERT_TRUE(i.h == 0x11223344);
};
