#include <crypto/crypto.hpp>
#include <crypto/crypto_libsodium.hpp>
#include <llarp_test.hpp>
#include <path/path.hpp>
#include <service/address.hpp>
#include <service/identity.hpp>
#include <service/intro_set.hpp>
#include <util/time.hpp>

#include <crypto/mock_crypto.hpp>
#include <test_util.hpp>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

using namespace llarp;
using namespace testing;

struct HiddenServiceTest : public test::LlarpTest<>
{
  service::Identity ident;
};

TEST_F(HiddenServiceTest, TestGenerateIntroSet)
{
  service::Address addr;
  ASSERT_TRUE(ident.pub.CalculateAddress(addr.as_array()));
  service::IntroSet I;
  auto now = time_now_ms();
  I.T      = now;
  while(I.I.size() < 10)
  {
    service::Introduction intro;
    intro.expiresAt = now + (path::default_lifetime / 2);
    intro.router.Randomize();
    intro.pathID.Randomize();
    I.I.emplace_back(std::move(intro));
  }

  EXPECT_CALL(m_crypto, sign(I.Z, _, _)).WillOnce(Return(true));
  EXPECT_CALL(m_crypto, verify(_, _, I.Z)).WillOnce(Return(true));

  ASSERT_TRUE(ident.SignIntroSet(I, now));
  ASSERT_TRUE(I.Verify(now));
}

TEST_F(HiddenServiceTest, TestAddressToFromString)
{
  auto str = ident.pub.Addr().ToString();
  service::Address addr;
  ASSERT_TRUE(addr.FromString(str));
  ASSERT_TRUE(addr == ident.pub.Addr());
}

struct ServiceIdentityTest : public test::LlarpTest<>
{
  ServiceIdentityTest()
  {
  }
};

template < typename Arg >
std::function< void(Arg&) >
FillArg(byte_t val)
{
  return [=](Arg& arg) { arg.Fill(val); };
}

TEST_F(ServiceIdentityTest, EnsureKeys)
{
  fs::path p = test::randFilename();
  ASSERT_FALSE(fs::exists(fs::status(p)));

  test::FileGuard guard(p);

  EXPECT_CALL(m_crypto, encryption_keygen(_))
      .WillOnce(WithArg< 0 >(FillArg< SecretKey >(0x01)));

  EXPECT_CALL(m_crypto, identity_keygen(_))
      .WillOnce(WithArg< 0 >(FillArg< SecretKey >(0x02)));

  EXPECT_CALL(m_crypto, pqe_keygen(_))
      .WillOnce(WithArg< 0 >(FillArg< PQKeyPair >(0x03)));

  service::Identity identity;
  ASSERT_TRUE(identity.EnsureKeys(p.string(), false));
  ASSERT_TRUE(fs::exists(fs::status(p)));

  // Verify what is on disk is what is what was generated
  service::Identity other;
  // No need to set more mocks, as we shouldn't need to re-keygen
  ASSERT_TRUE(other.EnsureKeys(p.string(), false));
  ASSERT_EQ(identity, other);
}

TEST_F(ServiceIdentityTest, EnsureKeysDir)
{
  fs::path p = test::randFilename();
  ASSERT_FALSE(fs::exists(fs::status(p)));

  test::FileGuard guard(p);
  std::error_code code;
  ASSERT_TRUE(fs::create_directory(p, code)) << code;

  service::Identity identity;
  ASSERT_FALSE(identity.EnsureKeys(p.string(), false));
}

TEST_F(ServiceIdentityTest, EnsureKeysBrokenFile)
{
  fs::path p = test::randFilename();
  ASSERT_FALSE(fs::exists(fs::status(p)));

  test::FileGuard guard(p);
  std::error_code code;

  std::fstream file;
  file.open(p.string(), std::ios::out);
  ASSERT_TRUE(file.is_open()) << p;
  file.close();

  service::Identity identity;
  ASSERT_FALSE(identity.EnsureKeys(p.string(), false));
}
