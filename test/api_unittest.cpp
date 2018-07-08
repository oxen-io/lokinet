#include <gtest/gtest.h>
#include <llarp/api/messages.hpp>

class APITest : public ::testing::Test
{
 public:
  llarp_crypto crypto;
  std::string apiPassword = "password";
  APITest()
  {
    llarp_crypto_libsodium_init(&crypto);
  }

  ~APITest()
  {
  }
};

TEST_F(APITest, TestMessageWellFormed)
{
  llarp::api::CreateSessionMessage msg;
  msg.msgID     = 0;
  msg.sessionID = 12345;
  msg.CalculateHash(&crypto, apiPassword);
  ASSERT_TRUE(msg.IsWellFormed(&crypto, apiPassword));
};