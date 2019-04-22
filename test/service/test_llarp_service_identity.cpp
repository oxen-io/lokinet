#include <crypto/crypto.hpp>
#include <crypto/crypto_libsodium.hpp>
#include <path/path.hpp>
#include <service/address.hpp>
#include <service/identity.hpp>
#include <service/intro_set.hpp>
#include <util/time.hpp>

#include <gtest/gtest.h>

struct HiddenServiceTest : public ::testing::Test
{
  llarp::sodium::CryptoLibSodium crypto;
  llarp::service::Identity ident;

  HiddenServiceTest()
  {
  }

  llarp::Crypto*
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
  ASSERT_TRUE(ident.pub.CalculateAddress(addr.as_array()));
  llarp::service::IntroSet I;
  auto now = llarp::time_now_ms();
  I.T      = now;
  while(I.I.size() < 10)
  {
    llarp::service::Introduction intro;
    intro.expiresAt = now + (llarp::path::default_lifetime / 2);
    intro.router.Randomize();
    intro.pathID.Randomize();
    I.I.emplace_back(std::move(intro));
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
