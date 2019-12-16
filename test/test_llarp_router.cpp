#include <router/router.hpp>

#include <crypto/crypto.hpp>
#include <crypto/crypto_libsodium.hpp>
#include <llarp_test.hpp>

#include <functional>
#include <random>

#include <test_util.hpp>
#include <gtest/gtest.h>

using namespace ::llarp;
using namespace ::testing;

/*
 * TODO: reimplement
 *
using FindOrCreateFunc = std::function< bool(const fs::path &, SecretKey &) >;

struct FindOrCreate : public test::LlarpTest<>,
                      public WithParamInterface< FindOrCreateFunc >
{
};

// Concerns
// - file missing
// - file empty
// - happy path

TEST_P(FindOrCreate, find_file_missing)
{
  // File missing. Should create a new file
  SecretKey key;
  fs::path p = test::randFilename();
  ASSERT_FALSE(fs::exists(fs::status(p)));

  test::FileGuard guard(p);

  EXPECT_CALL(m_crypto, encryption_keygen(_))
      .Times(AtMost(1))
      .WillRepeatedly(Invoke(&test::keygen< SecretKey >));

  EXPECT_CALL(m_crypto, identity_keygen(_))
      .Times(AtMost(1))
      .WillRepeatedly(Invoke(&test::keygen< SecretKey >));

  ASSERT_TRUE(GetParam()(p, key));
  ASSERT_TRUE(fs::exists(fs::status(p)));
  ASSERT_FALSE(key.IsZero());
}

TEST_P(FindOrCreate, find_file_empty)
{
  // File empty.
  SecretKey key;
  fs::path p = test::randFilename();
  ASSERT_FALSE(fs::exists(fs::status(p)));

  std::fstream f;
  f.open(p.string(), std::ios::out);
  f.close();

  test::FileGuard guard(p);

  ASSERT_FALSE(GetParam()(p, key));
  // Verify we didn't delete an invalid file
  ASSERT_TRUE(fs::exists(fs::status(p)));
}

TEST_P(FindOrCreate, happy_path)
{
  // happy path.
  SecretKey key;
  fs::path p = test::randFilename();
  ASSERT_FALSE(fs::exists(fs::status(p)));

  std::ofstream f;
  f.open(p.string(), std::ios::out);
  std::fill_n(std::ostream_iterator< byte_t >(f), key.size(), 0x20);
  f.close();

  test::FileGuard guard(p);

  ASSERT_TRUE(GetParam()(p, key));
  // Verify we didn't delete the file
  ASSERT_TRUE(fs::exists(fs::status(p)));
}

FindOrCreateFunc findOrCreateFunc[] = {llarp_findOrCreateEncryption,
                                       llarp_findOrCreateIdentity};

INSTANTIATE_TEST_CASE_P(TestRouter, FindOrCreate,
                        ::testing::ValuesIn(findOrCreateFunc), );
*/
