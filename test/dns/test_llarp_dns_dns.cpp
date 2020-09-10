#include <catch2/catch.hpp>

#include <dns/dns.hpp>
#include <dns/message.hpp>
#include <dns/name.hpp>
#include <dns/rr.hpp>
#include <net/net.hpp>
#include <net/ip.hpp>
#include <util/buffer.hpp>

#include <algorithm>

constexpr auto tld = ".loki";

TEST_CASE("Test Has TLD", "[dns]")
{
  llarp::dns::Question question;
  question.qname = "a.loki.";
  CHECK(question.HasTLD(tld));
  question.qname = "a.loki..";
  CHECK(not question.HasTLD(tld));
  question.qname = "bepis.loki.";
  CHECK(question.HasTLD(tld));
  question.qname = "bepis.logi.";
  CHECK(not question.HasTLD(tld));
  question.qname = "a.net.";
  CHECK(not question.HasTLD(tld));
  question.qname = "a.boki.";
  CHECK(not question.HasTLD(tld));
  question.qname = "t.co.";
  CHECK(not question.HasTLD(tld));
};

TEST_CASE("Test Is Localhost.loki", "[dns]")
{
  llarp::dns::Question question;

  question.qname = "localhost.loki.";
  CHECK(question.IsLocalhost());
  question.qname = "foo.localhost.loki.";
  CHECK(question.IsLocalhost());
  question.qname = "foo.bar.localhost.loki.";
  CHECK(question.IsLocalhost());

  question.qname = "something.loki.";
  CHECK(not question.IsLocalhost());
  question.qname = "localhost.something.loki.";
  CHECK(not question.IsLocalhost());
  question.qname = "notlocalhost.loki.";
  CHECK(not question.IsLocalhost());
};

TEST_CASE("Test Get Subdomains" , "[dns]")
{
  llarp::dns::Question question;
  std::string expected;

  question.qname = "localhost.loki.";
  expected = "";
  CHECK(question.Subdomains() == expected);

  question.qname = "foo.localhost.loki.";
  expected = "foo";
  CHECK(question.Subdomains() == expected);

  question.qname = "foo.bar.localhost.loki.";
  expected = "foo.bar";
  CHECK(question.Subdomains() == expected);

  // not legal, but test it anyway
  question.qname = ".localhost.loki.";
  expected = "";
  CHECK(question.Subdomains() == expected);

  question.qname = ".loki.";
  expected = "";
  CHECK(question.Subdomains() == expected);

  question.qname = "loki.";
  expected = "";
  CHECK(question.Subdomains() == expected);

  question.qname = ".";
  expected = "";
  CHECK(question.Subdomains() == expected);

  question.qname = "";
  expected = "";
  CHECK(question.Subdomains() == expected);
};

TEST_CASE("Test PTR records", "[dns]")
{
  llarp::huint128_t ip = {0};
  llarp::huint128_t expected =
      llarp::net::ExpandV4(llarp::ipaddr_ipv4_bits(10, 10, 10, 1));
  CHECK(llarp::dns::DecodePTR("1.10.10.10.in-addr.arpa.", ip));
  CHECK(ip == expected);
}

TEST_CASE("Test Serialize Header", "[dns]")
{
  std::array<byte_t, 1500> data{};
  llarp_buffer_t buf(data);
  llarp::dns::MessageHeader hdr, other;
  hdr.id       = 0x1234;
  hdr.fields   = (1 << 15);
  hdr.qd_count = 1;
  hdr.an_count = 1;
  hdr.ns_count = 0;
  hdr.ar_count = 0;
  
  CHECK(hdr.Encode(&buf));
  CHECK((buf.cur - buf.base) == llarp::dns::MessageHeader::Size);

  // rewind
  buf.cur = buf.base;

  CHECK(other.Decode(&buf));
  CHECK(hdr == other);
  CHECK(other.id == 0x1234);
  CHECK(other.fields == (1 << 15));
}

TEST_CASE("Test Serialize Name" , "[dns]")
{
  const llarp::dns::Name_t name     = "whatever.tld";
  const llarp::dns::Name_t expected = "whatever.tld.";
  llarp::dns::Name_t other;
  std::array<byte_t, 1500> data{};
  llarp_buffer_t buf(data);
  
  CHECK(llarp::dns::EncodeName(&buf, name));

  buf.cur = buf.base;
  
  CHECK(buf.base[0] == 8);
  CHECK(buf.base[1] == 'w');
  CHECK(buf.base[2] == 'h');
  CHECK(buf.base[3] == 'a');
  CHECK(buf.base[4] == 't');
  CHECK(buf.base[5] == 'e');
  CHECK(buf.base[6] == 'v');
  CHECK(buf.base[7] == 'e');
  CHECK(buf.base[8] == 'r');
  CHECK(buf.base[9] == 3);
  CHECK(buf.base[10] == 't');
  CHECK(buf.base[11] == 'l');
  CHECK(buf.base[12] == 'd');
  CHECK(buf.base[13] == 0);
  CHECK(llarp::dns::DecodeName(&buf, other));
  CHECK(expected == other);
}

TEST_CASE("Test serialize question", "[dns]")
{
  const std::string name          = "whatever.tld";
  const std::string expected_name = name + ".";
  llarp::dns::Question q, other;

  std::array<byte_t, 1500> data{};
  llarp_buffer_t buf(data);
  
  q.qname  = name;
  q.qclass = 1;
  q.qtype  = 1;
  CHECK(q.Encode(&buf));

  buf.cur = buf.base;
  
  CHECK(other.Decode(&buf));
  CHECK(other.qname == expected_name);
  CHECK(q.qclass == other.qclass);
  CHECK(q.qtype == other.qtype);
}

TEST_CASE("Test Encode/Decode RData" , "[dns]")
{
  std::array<byte_t, 1500> data{};
  llarp_buffer_t buf(data);

  static constexpr size_t rdatasize = 32;
  llarp::dns::RR_RData_t rdata(rdatasize);
  std::fill(rdata.begin(), rdata.end(), 'a');
  llarp::dns::RR_RData_t other_rdata;

  CHECK(llarp::dns::EncodeRData(&buf, rdata));
  CHECK(buf.cur - buf.base == rdatasize + sizeof(uint16_t));

  buf.cur = buf.base;
  
  CHECK(llarp::dns::DecodeRData(&buf, other_rdata));
  CHECK(rdata == other_rdata);
}
