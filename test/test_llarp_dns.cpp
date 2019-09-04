#include <gtest/gtest.h>

#include <dns.hpp>
#include <dnsc.hpp>
#include <llarp.h>                // for llarp_main_init
#include <net/net.hpp>            // for llarp::Addr
#include <util/thread/logic.hpp>  // for threadpool/llarp::Logic

struct DNSTest : public ::testing::Test
{
  unsigned char buf[47] = {
      0x00, 0x01,                                // first short
      0x01, 0x00, 0x00,                          // combined fields
      0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,  // last 4 shorts
      // question (is 18 bytes long)
      0x04,                                      // 4 letters
      0x6C, 0x6F, 0x6B, 0x69,                    // loki
      0x07,                                      // 7 letters
      0x6E, 0x65, 0x74, 0x77, 0x6F, 0x72, 0x6B,  // network
      0x00,                                      // end
      0x00, 0x01,                                // type (a 1/ptr 12)
      0x00, 0x01,                                // class (1 = internet)
                                                 // 30th byte
                                                 // Answer (is 16 bytes long)
      0xc0, 0x0c, 0x00, 0x01, 0x00, 0x01,        // name, type, class
      0x00, 0x00, 0x08, 0x4b,                    // ttl 2123
      0x00, 0x04,                                // rdLen
      0x45, 0x10, 0xd1, 0x02,                    // an ip address
      // extra
      0x00  // null terminator (probably don't need this, just added it)
  };
  llarp_buffer_t buffer_t;

  DNSTest()
  {
    this->buffer_t.base = (byte_t *)this->buf;
    this->buffer_t.cur  = buffer_t.base;
    this->buffer_t.sz   = 47;
  }

  void
  SetUp()
  {
    llarp::SetLogLevel(
        llarp::eLogNone);  // turn off logging to keep gtest output pretty
    /*
     const char *url = "loki.network";
     struct dns_query *packet = build_dns_packet((char *)url, 1, 1); // id 1,
     type 1 (A)

     unsigned int length = packet->length;
     char *buffer = (char *)packet->request;
     char hex_buffer[length * 5 + 1];
     hex_buffer[length * 5] = 0;
     for(unsigned int j = 0; j < length; j++)
     sprintf(&hex_buffer[5 * j], "0x%02X,", ((const char *)buffer)[j]);
     printf("Generated [%u] bytes: [%s]\n", length, hex_buffer);
     */
  }
};

// test puts/gets
// test code_domain / getDNSstring
TEST_F(DNSTest, TestDecodeDNSstring)
{
  char *buffer = (char *)this->buf;
  buffer += 12;  // skip header
  uint32_t pos    = 0;
  std::string res = getDNSstring(buffer, &pos);
  ASSERT_TRUE(res == "loki.network");
}

TEST_F(DNSTest, TestCodeDomain)
{
  char buffer[16];
  llarp::Zero(buffer, 16);
  char *write_buffer = buffer;
  std::string url    = "bob.com";
  code_domain(write_buffer, url);

  char hex_buffer[16 * 3 + 1];
  hex_buffer[16 * 3] = 0;
  for(unsigned int j = 0; j < 16; j++)
    sprintf(&hex_buffer[3 * j], "%02X ", ((const char *)buffer)[j]);
  // printf("first 16 [%s]", hex_buffer);

  std::string expected_result =
      "03 62 6F 62 03 63 6F 6D 00 00 00 00 00 00 00 00 ";
  ASSERT_TRUE(hex_buffer == expected_result);
}

// test decoders
TEST_F(DNSTest, TestDecodeHdr)
{
  dns_msg_header hdr;
  ASSERT_TRUE(decode_hdr(&this->buffer_t, &hdr));
  // rewind
  buffer_t.cur = buffer_t.base;
  /*
  printf("id[%d]", hdr->id);
  printf("qr[%d]", hdr->qr);
  printf("oc[%d]", hdr->opcode);
  printf("aa[%d]", hdr->aa);
  printf("tc[%d]", hdr->tc);
  printf("rd[%d]", hdr->rd);
  printf("ra[%d]", hdr->ra);
  printf("z [%d]", hdr->z);
  printf("ad[%d]", hdr->ad);
  printf("cd[%d]", hdr->cd);
  printf("rc[%d]", hdr->rcode);
  printf("qd[%d]", hdr->qdCount);
  printf("an[%d]", hdr->anCount);
  printf("ns[%d]", hdr->nsCount);
  printf("ar[%d]", hdr->arCount);
  */
  ASSERT_TRUE(hdr.id == 1);
  ASSERT_TRUE(hdr.qr == 0);
  ASSERT_TRUE(hdr.opcode == 0);
  ASSERT_TRUE(hdr.aa == 0);
  ASSERT_TRUE(hdr.tc == 0);
  ASSERT_TRUE(hdr.rd == 0);
  ASSERT_TRUE(hdr.ra == 0);
  ASSERT_TRUE(hdr.z == 0);
  ASSERT_TRUE(hdr.ad == 0);
  ASSERT_TRUE(hdr.cd == 0);
  ASSERT_TRUE(hdr.rcode == 0);
  ASSERT_TRUE(hdr.qdCount == 1);
  ASSERT_TRUE(hdr.anCount == 1);
  ASSERT_TRUE(hdr.nsCount == 0);
  ASSERT_TRUE(hdr.arCount == 0);
}

TEST_F(DNSTest, TestDecodeQuestion)
{
  char *buffer = (char *)this->buf;
  buffer += 12;  // skip header
  uint32_t pos               = 0;
  dns_msg_question *question = decode_question(buffer, &pos);
  // printf("name[%s]", question->name.c_str());
  // printf("type[%d]", question->type);
  // printf("qClass[%d]", question->qClass);
  std::string url = "loki.network";
  ASSERT_TRUE(question->name == url);
  ASSERT_TRUE(question->type == 1);
  ASSERT_TRUE(question->qClass == 1);
}

TEST_F(DNSTest, TestDecodeAnswer)
{
  const char *const buffer = (const char *)this->buf;
  uint32_t pos             = 12;
  std::string url          = "loki.network";
  pos += url.length() + 2 + 4;  // skip question (string + 2 shorts)

  dns_msg_answer *answer = decode_answer(buffer, &pos);
  /*
  printf("type[%d]", answer->type);
  printf("aClass[%d]", answer->aClass);
  printf("ttl[%d]", answer->ttl);
  printf("rdLen[%d]", answer->rdLen);
  printf("[%hhu].[%hhu].[%hhu].[%hhu]", answer->rData[0], answer->rData[1],
  answer->rData[2], answer->rData[3]);
  */
  ASSERT_TRUE(answer->name == url);
  ASSERT_TRUE(answer->type == 1);
  ASSERT_TRUE(answer->aClass == 1);
  ASSERT_TRUE(answer->ttl == 2123);
  ASSERT_TRUE(answer->rdLen == 4);
  ASSERT_TRUE(answer->rData[0] == 69);
  ASSERT_TRUE(answer->rData[1] == 16);
  ASSERT_TRUE(answer->rData[2] == 209);
  ASSERT_TRUE(answer->rData[3] == 2);
}

/// UDP handling configuration
struct llarp_udp_io_mock
{
  /// set after added
  int fd;
  void *user;
  void *impl;
  struct llarp_ev_loop *parent;
  /// called every event loop tick after reads
  void (*tick)(struct llarp_udp_io *);
  // sockaddr * is the source
  void (*recvfrom)(struct llarp_udp_io *, const struct sockaddr *, const void *,
                   ssize_t);
};

// will have to mock udp and intercept the sendto call...
// test llarp_handle_dns_recvfrom
TEST_F(DNSTest, handleDNSrecvFrom)
{
  llarp_udp_io_mock udp;
  sockaddr addr;
  std::array< byte_t, 16 > buffer;
  std::fill(buffer.begin(), buffer.end(), 0);
  // hdr->qr decides dnsc (1) or dnsd (0)
  llarp_handle_dns_recvfrom((llarp_udp_io *)&udp, &addr,
                            ManagedBuffer(llarp_buffer_t(buffer)));
  // llarp_handle_dnsc_recvfrom
  // llarp_handle_dnsd_recvfrom
}
