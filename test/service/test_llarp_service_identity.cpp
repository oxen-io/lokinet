#include <crypto/crypto.hpp>
#include <crypto/crypto_libsodium.hpp>
#include <sodium/crypto_scalarmult_ed25519.h>
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

  using ::testing::Matcher;
  EXPECT_CALL(m_crypto, sign(I.Z, Matcher<const SecretKey &>(_), _)).WillOnce(Return(true));
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

  EXPECT_CALL(m_crypto, derive_subkey_private(_, _, _, _))
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

TEST_F(RealCryptographyTest, TestKnownDerivation)
{
  // These values came out of Tor's test code, so that we can confirm we are doing the same blinding
  // subkey crypto math as Tor.  Our hash value is generated differently so we use the hash from a
  // Tor random test suite run.

  AlignedBuffer<32> seed{{
    0x11, 0x68, 0xae, 0xa6, 0x62, 0x26, 0x6c, 0x53, 0x69, 0x9f, 0xe7, 0xd9, 0xbb, 0xff, 0xf6, 0x8e,
    0x58, 0x22, 0xde, 0x90, 0x4b, 0x91, 0x28, 0x5a, 0x7c, 0x41, 0xcc, 0x7c, 0x36, 0xb4, 0xf5, 0xa0 }};
  AlignedBuffer<32> root_key_data{{
    0x40, 0x64, 0x32, 0x11, 0x19, 0xfc, 0xe8, 0x27, 0x9d, 0x3f, 0xd6, 0xe9, 0xc8, 0x4c, 0x5a, 0xea,
    0x32, 0xd4, 0xe3, 0x97, 0x4a, 0xe4, 0x00, 0xd0, 0xd8, 0x36, 0xc2, 0x0e, 0xe4, 0xa2, 0x7c, 0x6c }};
  AlignedBuffer<32> root_pub_data{{
    0x69, 0x8b, 0x43, 0xbb, 0x54, 0xeb, 0x31, 0x2e, 0x5a, 0x07, 0x3f, 0x59, 0x5f, 0x1a, 0xbf, 0xe3,
    0x95, 0xf2, 0x7a, 0x6d, 0x1d, 0x64, 0x5c, 0x4b, 0x10, 0x3f, 0xa2, 0xf5, 0xe6, 0x97, 0x5c, 0x70 }};
  AlignedBuffer<32> hash{{
    0x22, 0x41, 0xca, 0x66, 0x21, 0x4c, 0x75, 0x40, 0x65, 0x57, 0x9e, 0x81, 0x8c, 0x70, 0x15, 0x2a,
    0x71, 0xb6, 0xc1, 0x67, 0x3f, 0x3b, 0x4b, 0x22, 0x31, 0xed, 0x22, 0x30, 0x2e, 0x2a, 0x23, 0x8e }};
  AlignedBuffer<32> derived_key_data{{
    0xbd, 0x0c, 0x55, 0x32, 0x62, 0x89, 0x61, 0xea, 0x86, 0x10, 0xd2, 0x27, 0x18, 0x51, 0xc0, 0x5e,
    0x0e, 0xb1, 0x5a, 0x45, 0xb7, 0xb6, 0x16, 0xbe, 0x37, 0xba, 0x9a, 0x34, 0x39, 0xc4, 0xd0, 0x07 }};
  AlignedBuffer<32> derived_pub_data{{
    0xa0, 0x72, 0x62, 0x22, 0xd7, 0xc0, 0x91, 0x49, 0xe5, 0xe7, 0x86, 0x0d, 0xc1, 0x53, 0x14, 0x02,
    0xe9, 0x96, 0xb8, 0xd8, 0x93, 0xb9, 0x2f, 0xe9, 0xc8, 0xf6, 0xf0, 0x5d, 0xe2, 0x30, 0x06, 0x48 }};

  SecretKey root{seed};
  ASSERT_EQ(root.toPublic(), PubKey{root_pub_data});

  PrivateKey root_key;
  ASSERT_TRUE(root.toPrivate(root_key));
  ASSERT_EQ(root_key, PrivateKey{root_key_data});

  auto crypto = CryptoManager::instance();

  PrivateKey aprime; // a'
  ASSERT_TRUE(crypto->derive_subkey_private(aprime, root, 0, &hash));
  ASSERT_EQ(aprime, PrivateKey{derived_key_data});

  PubKey Aprime; // A'
  ASSERT_TRUE(crypto->derive_subkey(Aprime, root.toPublic(), 0, &hash));
  ASSERT_EQ(Aprime, PubKey{derived_pub_data});
}

TEST_F(RealCryptographyTest, TestGenerateDeriveKey)
{
  auto crypto = CryptoManager::instance();
  SecretKey root_key;
  crypto->identity_keygen(root_key);

  PrivateKey root_privkey;
  ASSERT_TRUE(root_key.toPrivate(root_privkey));

  PrivateKey a;
  PubKey A;
  ASSERT_TRUE(root_key.toPrivate(a));
  ASSERT_TRUE(a.toPublic(A));
  ASSERT_EQ(A, root_key.toPublic());

  {
    // paranoid check to ensure this works as expected
    PubKey aB;
    crypto_scalarmult_ed25519_base(aB.data(), a.data());
    ASSERT_EQ(A, aB);
  }

  PrivateKey aprime; // a'
  ASSERT_TRUE(crypto->derive_subkey_private(aprime, root_key, 1));

  PubKey Aprime; // A'
  ASSERT_TRUE(crypto->derive_subkey(Aprime, A, 1));

  // We should also be able to derive A' via a':
  PubKey Aprime_alt;
  ASSERT_TRUE(aprime.toPublic(Aprime_alt));

  ASSERT_EQ(Aprime, Aprime_alt);
}

TEST_F(RealCryptographyTest, TestSignUsingDerivedKey)
{
  auto crypto = CryptoManager::instance();
  SecretKey root_key;
  crypto->identity_keygen(root_key);

  PrivateKey root_privkey;
  root_key.toPrivate(root_privkey);

  PrivateKey a;
  PubKey A;
  root_key.toPrivate(a);
  a.toPublic(A);

  PrivateKey aprime; // a'
  crypto->derive_subkey_private(aprime, root_key, 1);

  PubKey Aprime; // A'
  crypto->derive_subkey(Aprime, A, 1);

  std::string dummystr = "Jeff loves one-letter variable names.";
  llarp_buffer_t buf(dummystr.data(), dummystr.size());

  Signature sig;
  ASSERT_TRUE(crypto->sign(sig, aprime, buf));

  ASSERT_TRUE(crypto->verify(Aprime, buf, sig));
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
