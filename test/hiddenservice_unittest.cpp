#include <gtest/gtest.h>
#include <llarp/service.hpp>

struct HiddenServiceTest : public ::testing::Test
{
  llarp_crypto crypto;
  llarp::service::Identity ident;

  HiddenServiceTest()
  {
    llarp_crypto_libsodium_init(&crypto);
  }

  llarp_crypto*
  Crypto()
  {
    return &crypto;
  }

  void
  SetUp()
  {
    ident.RegenerateKeys(Crypto());
    ident.pub.vanity.Randomize();
  }
};

TEST_F(HiddenServiceTest, TestGenerateIntroSet)
{
  llarp::service::Address addr;
  ASSERT_TRUE(ident.pub.CalculateAddress(addr));
  llarp::service::IntroSet I;
  while(I.I.size() < 10)
  {
    llarp::service::Introduction intro;
    intro.expiresAt = 1000;
    intro.router.Randomize();
    intro.pathID.Randomize();
    I.I.push_back(intro);
  }
  ASSERT_TRUE(ident.SignIntroSet(I, Crypto()));
  ASSERT_TRUE(I.VerifySignature(Crypto()));
};