#include <router/router.hpp>

#include <crypto/crypto.hpp>
#include <crypto/crypto_libsodium.hpp>

#include <functional>
#include <random>

#include <test_util.hpp>
#include <gtest/gtest.h>

using FindOrCreateFunc = std::function< bool(llarp::Crypto *, const fs::path &,
                                             llarp::SecretKey &) >;

struct FindOrCreate : public ::testing::TestWithParam< FindOrCreateFunc >
{
  FindOrCreate()
  {
  }

  llarp::sodium::CryptoLibSodium crypto;
};

// Concerns
// - file missing
// - file empty
// - happy path

TEST_P(FindOrCreate, find_file_missing)
{
  // File missing. Should create a new file
  llarp::SecretKey key;
  fs::path p = llarp::test::randFilename();
  ASSERT_FALSE(fs::exists(fs::status(p)));

  llarp::test::FileGuard guard(p);

  ASSERT_TRUE(GetParam()(&crypto, p, key));
  ASSERT_TRUE(fs::exists(fs::status(p)));
  ASSERT_FALSE(key.IsZero());
}

TEST_P(FindOrCreate, find_file_empty)
{
  // File empty.
  llarp::SecretKey key;
  fs::path p = llarp::test::randFilename();
  ASSERT_FALSE(fs::exists(fs::status(p)));

  std::fstream f;
  f.open(p.string(), std::ios::out);
  f.close();

  llarp::test::FileGuard guard(p);

  ASSERT_FALSE(GetParam()(&crypto, p, key));
  // Verify we didn't delete an invalid file
  ASSERT_TRUE(fs::exists(fs::status(p)));
}

TEST_P(FindOrCreate, happy_path)
{
  // happy path.
  llarp::SecretKey key;
  fs::path p = llarp::test::randFilename();
  ASSERT_FALSE(fs::exists(fs::status(p)));

  std::ofstream f;
  f.open(p.string(), std::ios::out);
  std::fill_n(std::ostream_iterator< byte_t >(f), key.size(), 0x20);
  f.close();

  llarp::test::FileGuard guard(p);

  ASSERT_TRUE(GetParam()(&crypto, p, key));
  // Verify we didn't delete the file
  ASSERT_TRUE(fs::exists(fs::status(p)));
}

FindOrCreateFunc findOrCreateFunc[] = {llarp_findOrCreateEncryption,
                                       llarp_findOrCreateIdentity};

INSTANTIATE_TEST_CASE_P(TestRouter, FindOrCreate,
                        ::testing::ValuesIn(findOrCreateFunc));
