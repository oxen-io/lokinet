#include <gtest/gtest.h>

#include <llarp/service.hpp>
#include <llarp/time.hpp>

struct HiddenServiceTest : public ::testing::Test
{
  llarp_crypto crypto;
  llarp::service::Identity ident;

  HiddenServiceTest()
  {
    llarp_crypto_init(&crypto);
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
    ident.pub.RandomizeVanity();
    ident.pub.UpdateAddr();
  }
};

TEST_F(HiddenServiceTest, TestGenerateIntroSet)
{
  llarp::service::Address addr;
  ASSERT_TRUE(ident.pub.CalculateAddress(addr.data()));
  llarp::service::IntroSet I;
  auto now = llarp::time_now_ms();
  I.T      = now;
  while(I.I.size() < 10)
  {
    llarp::service::Introduction intro;
    intro.expiresAt = now + (DEFAULT_PATH_LIFETIME / 2);
    intro.router.Randomize();
    intro.pathID.Randomize();
    I.I.push_back(intro);
  }
  ASSERT_TRUE(ident.SignIntroSet(I, Crypto(), now));
  ASSERT_TRUE(I.Verify(Crypto(), now));
};

TEST_F(HiddenServiceTest, TestAddressToFromString)
{
  auto str = ident.pub.Addr().ToString();
  llarp::service::Address addr;
  ASSERT_TRUE(addr.FromString(str));
  ASSERT_TRUE(addr == ident.pub.Addr());
}
