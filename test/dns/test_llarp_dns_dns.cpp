#include <catch2/catch.hpp>
#include <climits>
#include <llarp/dns/message.hpp>
#include <llarp/dns/name.hpp>
#include <llarp/dns/rr.hpp>
#include <llarp/net/net.hpp>
#include <llarp/net/ip.hpp>
#include <llarp/util/buffer.hpp>

#include <algorithm>


TEST_CASE("Test Has TLD", "[dns]")
{
                       
    for(std::string name : {"a.loki.", "bepis.loki."})
    {
        SECTION(name)
        {
            llarp::dns::Question question{name, llarp::dns::RRType::A};      
            CHECK(question.tld() == "loki");
        }
    }
    
    for(std::string name : {"bepis.logi.", "a.net.", "a.boki.", "t.co."})
    {
        SECTION(name)
        {
            llarp::dns::Question question{name, llarp::dns::RRType::A};       
            CHECK(question.tld() != "loki");
        }
    }

    REQUIRE_THROWS(llarp::dns::Question{"a.loki..", llarp::dns::RRType::A});

};

TEST_CASE("Test Is Localhost.loki", "[dns]")
{
    for(std::string name : {"localhost.loki.", "foo.localhost.loki.",  "foo.bar.localhost.loki."})
    {
        SECTION(name)
        {  
            llarp::dns::Question question{name, llarp::dns::RRType::A};
            CHECK(question.IsLocalhost());
        }
    }
    for(std::string name: {"something.loki.", "localhost.something.loki.", "notlocalhost.loki."})
    {
        SECTION(name)
        {
            llarp::dns::Question question{name, llarp::dns::RRType::A};
            CHECK(not question.IsLocalhost());
        }
    }
};

TEST_CASE("Test Get Subdomains" , "[dns]")
{
    const std::map<std::string, std::string> values = {
        {"localhost.loki.", ""},
        {"foo.localhost.loki.", "foo"},
        {"foo.bar.localhost.loki.", "foo.bar"},
        {"loki.", ""},
    };
    for(const auto &[name, expected] : values)
    {
        SECTION(name)
        {
            llarp::dns::Question question{name, llarp::dns::RRType::A};
            CHECK(question.Subdomains() == expected);
        }
    }
    
    for(std::string invalid : {".localhost.loki.", ".loki.", ".", ""})
    {
        SECTION(invalid)
        {
            REQUIRE_THROWS(llarp::dns::Question{invalid, llarp::dns::RRType::A});
        }
    }

};

TEST_CASE("Test PTR records", "[dns]")
{
  llarp::huint128_t expected =
      llarp::net::ExpandV4(llarp::ipaddr_ipv4_bits(10, 10, 10, 1));
  auto ip = llarp::dns::DecodePTR("1.10.10.10.in-addr.arpa.");
  CHECK(ip);
  CHECK(*ip == expected);

  expected.h.upper = 0x0123456789abcdefUL;
  expected.h.lower = 0xeeee888812341234UL;
  ip = llarp::dns::DecodePTR("4.3.2.1.4.3.2.1.8.8.8.8.e.e.e.e.f.e.d.c.b.a.9.8.7.6.5.4.3.2.1.0.ip6.arpa.");
  CHECK(ip);
  CHECK(oxenc::to_hex(std::string_view{reinterpret_cast<char*>(&expected.h), 16}) ==
          oxenc::to_hex(std::string_view{reinterpret_cast<char*>(&ip->h), 16}));
  CHECK(*ip == expected);
}

TEST_CASE("Test Serialize Header", "[dns]")
{

  llarp::dns::MessageHeader hdr;
  hdr.id       = 0x1234;
  hdr.fields   = (1 << 15);
  hdr.qd_count = 1;
  hdr.an_count = 1;
  hdr.ns_count = 0;
  hdr.ar_count = 0;
  auto encoded = hdr.encode_dns();
  CHECK(encoded.size() == llarp::dns::MessageHeader::Size);

  llarp::byte_view_t raw{encoded};
  llarp::dns::MessageHeader other{raw};

  CHECK(raw.empty());
  CHECK(hdr == other);
  CHECK(other.id == 0x1234);
  CHECK(other.fields == (1 << 15));
}

TEST_CASE("Test Serialize Name" , "[dns]")
{
  const std::string name     = "whatever.tld";
  const std::string expected = "whatever.tld.";

  auto encoded = llarp::dns::encode_dns_labels(llarp::split(name, "."));
  CHECK(encoded[0] == 8);
  CHECK(encoded[1] == 'w');
  CHECK(encoded[2] == 'h');
  CHECK(encoded[3] == 'a');
  CHECK(encoded[4] == 't');
  CHECK(encoded[5] == 'e');
  CHECK(encoded[6] == 'v');
  CHECK(encoded[7] == 'e');
  CHECK(encoded[8] == 'r');
  CHECK(encoded[9] == 3);
  CHECK(encoded[10] == 't');
  CHECK(encoded[11] == 'l');
  CHECK(encoded[12] == 'd');
  CHECK(encoded[13] == 0);
  
  llarp::byte_view_t raw{encoded};

  std::string data;
  for(auto label : llarp::dns::decode_dns_labels(raw))
      data += fmt::format("{}.", label);
  CHECK(data == expected);
}

TEST_CASE("Test serialize question", "[dns]")
{
  const std::string name          = "whatever.tld";
  llarp::dns::Question q{name, llarp::dns::RRType::A};
  

  auto buf = q.encode_dns();

  CHECK(not buf.empty());

  llarp::byte_view_t raw{buf};
  llarp::dns::Question other{raw};
  
  CHECK(other.qname() == name);
  CHECK(q.qclass == other.qclass);
  CHECK(q.qtype == other.qtype);
  
}

TEST_CASE("Test Encode/Decode RData" , "[dns]")
{

}

TEST_CASE("Test reserved names", "[dns]")
{
    using namespace llarp::dns;
    for(std::string name : {"loki.loki", "snode.snode", "foo.loki.loki", "bar.snode.snode", "fug.wut.snode.loki", "as.df.po.iu.loki.snode"})
    {
        SECTION(name)
        {
            CHECK(NameIsReserved(name));
        }
    }
    for(std::string name : {"barsnode.loki", "alltheloki.loki", "fugsnode.snode", "sloki.snode"})
    {
        SECTION(name)
        {
            CHECK_FALSE(NameIsReserved(name));
        }
    }
}
