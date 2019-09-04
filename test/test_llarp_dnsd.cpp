#include <gtest/gtest.h>

#include <dnsd.hpp>

#include <llarp.h>                // for llarp_main_init
#include <net/net.hpp>            // for llarp::Addr
#include <util/thread/logic.hpp>  // for threadpool/llarp::Logic

unsigned int g_length = 0;
std::string g_result  = "";

ssize_t
test_sendto_dns_hook(__attribute__((unused)) void *sock,
                     __attribute__((unused)) const struct sockaddr *from,
                     ManagedBuffer buf)
{
  char *hex_buffer                  = new char[buf.underlying.sz * 3 + 1];
  hex_buffer[buf.underlying.sz * 3] = 0;
  for(unsigned int j = 0; j < buf.underlying.sz; j++)
    sprintf(&hex_buffer[3 * j], "%02X ", ((char *)buf.underlying.base)[j]);
  // printf("Got [%zu] bytes: [%s]\n", length, hex_buffer);
  g_result = hex_buffer;
  g_length = buf.underlying.sz;
  delete[] hex_buffer;
  return buf.underlying.sz;
}

struct llarpDNSdTest : public ::testing::Test
{
  dnsd_question_request test_request;

  llarpDNSdTest()
  {
  }
  void
  SetUp()
  {
    test_request.hdr.id          = 0;
    test_request.llarp           = true;  // we don't care about raw atm
    test_request.from            = nullptr;
    test_request.context         = nullptr;
    test_request.sendto_hook     = &test_sendto_dns_hook;
    test_request.question.name   = "loki.network";
    test_request.question.type   = 1;
    test_request.question.qClass = 1;
    g_result                     = "";  // reset test global
    g_length                     = 0;
  }
};

TEST_F(llarpDNSdTest, TestNxDomain)
{
  write404_dnss_response(&test_request);
  ASSERT_TRUE(g_length == 55);
  std::string expected_output =
      "00 00 FFF03 00 01 00 01 00 00 00 00 04 6C 6F 6B 69 07 6E 65 74 77 6F 72 "
      "6B 00 00 01 00 01 04 6C 6F 6B 69 07 6E 65 74 77 6F 72 6B 00 00 01 00 01 "
      "00 00 00 01 00 01 00 ";
  // TODO: rewrite this test
  // ASSERT_TRUE(expected_output == g_result);
}

TEST_F(llarpDNSdTest, TestAResponse)
{
  llarp::huint32_t hostRes = {0};
  // sockaddr hostRes;
  // llarp::Zero(&hostRes, sizeof(sockaddr));
  writesend_dnss_response(&hostRes, &test_request);
  ASSERT_TRUE(g_length == 58);
  std::string expected_output =
      "00 00 FFF00 00 01 00 01 00 00 00 00 04 6C 6F 6B 69 07 6E 65 74 77 6F 72 "
      "6B 00 00 01 00 01 04 6C 6F 6B 69 07 6E 65 74 77 6F 72 6B 00 00 01 00 01 "
      "00 00 00 01 00 04 00 00 00 00 ";
  // TODO: rewrite this test
  // ASSERT_TRUE(expected_output == g_result);
}

TEST_F(llarpDNSdTest, TestPTRResponse)
{
  writesend_dnss_revresponse("loki.network", &test_request);
  ASSERT_TRUE(g_length == 68);
  std::string expected_output =
      "00 00 FFF00 00 01 00 01 00 00 00 00 04 6C 6F 6B 69 07 6E 65 74 77 6F 72 "
      "6B 00 00 01 00 01 04 6C 6F 6B 69 07 6E 65 74 77 6F 72 6B 00 00 01 00 01 "
      "00 00 00 01 00 0E 04 6C 6F 6B 69 07 6E 65 74 77 6F 72 6B 00 ";
  // TODO: rewrite this test
  // ASSERT_TRUE(expected_output == g_result);
}

TEST_F(llarpDNSdTest, TestCname)
{
  writecname_dnss_response("test.cname", &test_request);
  ASSERT_TRUE(g_length == 122);
  std::string expected_output =
      "00 00 FFF00 00 01 00 01 00 01 00 01 04 6C 6F 6B 69 07 6E 65 74 77 6F 72 "
      "6B 00 00 01 00 01 04 6C 6F 6B 69 07 6E 65 74 77 6F 72 6B 00 00 05 00 01 "
      "00 00 00 01 00 0C 04 74 65 73 74 05 63 6E 61 6D 65 00 04 74 65 73 74 05 "
      "63 6E 61 6D 65 00 00 02 00 01 00 00 00 01 00 0A 03 6E 73 31 04 6C 6F 6B "
      "69 00 03 6E 73 31 04 6C 6F 6B 69 00 00 01 00 01 00 00 00 01 00 04 7F 00 "
      "00 01 ";
  // TODO: rewrite this test
  // ASSERT_TRUE(expected_output == g_result);
}
