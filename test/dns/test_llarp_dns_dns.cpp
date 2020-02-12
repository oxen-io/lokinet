#include <gtest/gtest.h>

#include <dns/dns.hpp>
#include <dns/message.hpp>
#include <dns/name.hpp>
#include <dns/rr.hpp>
#include <net/net.hpp>
#include <net/ip.hpp>
#include <util/buffer.hpp>

#include <algorithm>

struct DNSLibTest : public ::testing::Test
{
  const std::string tld = ".loki";
  std::array< byte_t, 1500 > mem;
  llarp_buffer_t buf;

  DNSLibTest() : buf(mem)
  {
    Rewind();
    std::fill(mem.begin(), mem.end(), '$');
  }

  void
  Rewind()
  {
    buf.cur = buf.base;
  }
};

TEST_F(DNSLibTest, TestHasTLD)
{
  llarp::dns::Question question;
  question.qname = "a.loki.";
  ASSERT_TRUE(question.HasTLD(tld));
  question.qname = "a.loki..";
  ASSERT_FALSE(question.HasTLD(tld));
  question.qname = "bepis.loki.";
  ASSERT_TRUE(question.HasTLD(tld));
  question.qname = "bepis.logi.";
  ASSERT_FALSE(question.HasTLD(tld));
  question.qname = "a.net.";
  ASSERT_FALSE(question.HasTLD(tld));
  question.qname = "a.boki.";
  ASSERT_FALSE(question.HasTLD(tld));
  question.qname = "t.co.";
  ASSERT_FALSE(question.HasTLD(tld));
};

TEST_F(DNSLibTest, TestPTR)
{
  llarp::huint128_t ip = {0};
  llarp::huint128_t expected =
      llarp::net::IPPacket::ExpandV4(llarp::ipaddr_ipv4_bits(10, 10, 10, 1));
  ASSERT_TRUE(llarp::dns::DecodePTR("1.10.10.10.in-addr.arpa.", ip));
  ASSERT_EQ(ip, expected);
}

TEST_F(DNSLibTest, TestSerializeHeader)
{
  llarp::dns::MessageHeader hdr, other;
  hdr.id       = 0x1234;
  hdr.fields   = (1 << 15);
  hdr.qd_count = 1;
  hdr.an_count = 1;
  hdr.ns_count = 0;
  hdr.ar_count = 0;
  ASSERT_TRUE(hdr.Encode(&buf));
  ASSERT_TRUE((buf.cur - buf.base) == llarp::dns::MessageHeader::Size);
  Rewind();
  ASSERT_TRUE(other.Decode(&buf));
  ASSERT_TRUE(hdr == other);
  ASSERT_TRUE(other.id == 0x1234);
  ASSERT_TRUE(other.fields == (1 << 15));
}

TEST_F(DNSLibTest, TestSerializeName)
{
  const llarp::dns::Name_t name     = "whatever.tld";
  const llarp::dns::Name_t expected = "whatever.tld.";
  llarp::dns::Name_t other;
  Rewind();
  ASSERT_TRUE(llarp::dns::EncodeName(&buf, name));
  Rewind();
  ASSERT_EQ(buf.base[0], 8);
  ASSERT_EQ(buf.base[1], 'w');
  ASSERT_EQ(buf.base[2], 'h');
  ASSERT_EQ(buf.base[3], 'a');
  ASSERT_EQ(buf.base[4], 't');
  ASSERT_EQ(buf.base[5], 'e');
  ASSERT_EQ(buf.base[6], 'v');
  ASSERT_EQ(buf.base[7], 'e');
  ASSERT_EQ(buf.base[8], 'r');
  ASSERT_EQ(buf.base[9], 3);
  ASSERT_EQ(buf.base[10], 't');
  ASSERT_EQ(buf.base[11], 'l');
  ASSERT_EQ(buf.base[12], 'd');
  ASSERT_EQ(buf.base[13], 0);
  ASSERT_TRUE(llarp::dns::DecodeName(&buf, other));
  ASSERT_EQ(expected, other);
}

TEST_F(DNSLibTest, TestSerializeQuestion)
{
  const std::string name          = "whatever.tld";
  const std::string expected_name = name + ".";
  llarp::dns::Question q, other;
  q.qname  = name;
  q.qclass = 1;
  q.qtype  = 1;
  ASSERT_TRUE(q.Encode(&buf));
  Rewind();
  ASSERT_TRUE(other.Decode(&buf));
  ASSERT_EQ(other.qname, expected_name);
  ASSERT_EQ(q.qclass, other.qclass);
  ASSERT_EQ(q.qtype, other.qtype);
}

/*
TEST_F(DNSLibTest, TestSerializeMessage)
{
  llarp::dns::Question expected_question;
  expected_question.qname  = "whatever.tld.";
  expected_question.qclass = 1;
  expected_question.qtype  = 1;
  llarp::dns::MessageHeader hdr, otherHdr;
  hdr.id       = 0xfeed;
  hdr.fields   = (1 << 15);
  hdr.qd_count = 1;
  hdr.an_count = 0;
  hdr.ns_count = 0;
  hdr.ar_count = 0;
  llarp::dns::Message m(hdr);
  m.hdr_id     = 0x1234;
  m.hdr_fields = (1 << 15);
  auto& q      = m.questions[0];
  q.qname      = "whatever.tld";
  q.qclass     = 1;
  q.qtype      = 1;
  m.AddINReply({1}, false);
  ASSERT_EQ(m.questions.size(), 1U);
  ASSERT_EQ(m.answers.size(), 1U);
  ASSERT_TRUE(m.Encode(&buf));

  Rewind();

  ASSERT_TRUE(otherHdr.Decode(&buf));
  llarp::dns::Message other(otherHdr);
  ASSERT_TRUE(buf.cur - buf.base == llarp::dns::MessageHeader::Size);
  ASSERT_TRUE(other.Decode(&buf));
  ASSERT_EQ(other.questions.size(), 1U);
  ASSERT_EQ(expected_question.qname, other.questions[0].qname);
  ASSERT_EQ(expected_question.qclass, other.questions[0].qclass);
  ASSERT_EQ(expected_question.qtype, other.questions[0].qtype);
  ASSERT_TRUE(expected_question == other.questions[0]);
  ASSERT_EQ(other.answers.size(), 1U);
  ASSERT_EQ(other.answers[0].rData.size(), 4U);
}
*/

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
}
