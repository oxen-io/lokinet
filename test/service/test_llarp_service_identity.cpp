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
  EXPECT_CALL(m_crypto, xchacha20(_, _, _)).WillOnce(Return(true));
  const auto maybe = ident.EncryptAndSignIntroSet(I, now);
  ASSERT_TRUE(maybe.has_value());
  ASSERT_TRUE(maybe->Verify(now));
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

  const SecretKey k;

  EXPECT_CALL(m_crypto, derive_subkey_secret(_, _, _))
      .WillRepeatedly(Return(true));

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

struct RealCryptographyTest : public ::testing::Test
{
  std::unique_ptr< CryptoManager > _manager;

  void
  SetUp()
  {
    _manager = std::make_unique< CryptoManager >(new sodium::CryptoLibSodium());
  }

  void
  TearDown()
  {
    _manager.reset();
  }
};

TEST_F(RealCryptographyTest, TestGenerateDeriveKey)
{
  SecretKey root_key;
  SecretKey sub_key;
  PubKey sub_pubkey;
  auto crypto = CryptoManager::instance();
  crypto->identity_keygen(root_key);
  ASSERT_TRUE(crypto->derive_subkey_secret(sub_key, root_key, 1));
  ASSERT_TRUE(crypto->derive_subkey(sub_pubkey, root_key.toPublic(), 1));
  ASSERT_EQ(sub_key.toPublic(), sub_pubkey);
}

TEST_F(RealCryptographyTest, TestEncryptAndSignIntroSet)
{
  service::Identity ident;
  ident.RegenerateKeys();
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

  const auto maybe = ident.EncryptAndSignIntroSet(I, now);
  ASSERT_TRUE(maybe.has_value());
  llarp::LogInfo("introset=", maybe.value());
  ASSERT_TRUE(maybe->Verify(now));
  PubKey blind_key;
  const PubKey root_key(addr.as_array());
  auto crypto = CryptoManager::instance();
  ASSERT_TRUE(crypto->derive_subkey(blind_key, root_key, 1));
  ASSERT_EQ(blind_key, root_key);
}
