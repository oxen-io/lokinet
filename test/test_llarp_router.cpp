
#include <router.hpp>
#include <crypto.hpp>

#include <functional>
#include <random>

#include <gtest/gtest.h>

std::string
randFilename()
{
  static const char alphabet[] = "abcdefghijklmnopqrstuvwxyz";

  std::random_device rd;
  std::uniform_int_distribution< size_t > dist{0, sizeof(alphabet) - 2};

  std::string filename;
  for(size_t i = 0; i < 5; ++i)
  {
    filename.push_back(alphabet[dist(rd)]);
  }

  filename.push_back('.');

  for(size_t i = 0; i < 5; ++i)
  {
    filename.push_back(alphabet[dist(rd)]);
  }

  return filename;
}

struct FileGuard
{
  const fs::path &p;

  explicit FileGuard(const fs::path &_p) : p(_p)
  {
  }

  ~FileGuard()
  {
    if(fs::exists(fs::status(p)))
    {
      fs::remove(p);
    }
  }
};

using FindOrCreateFunc = std::function< bool(llarp::Crypto *, const fs::path &,
                                             llarp::SecretKey &) >;

struct FindOrCreate : public ::testing::TestWithParam< FindOrCreateFunc >
{
  FindOrCreate() : crypto(llarp::Crypto::sodium{})
  {
  }

  llarp::Crypto crypto;
};

// Concerns
// - file missing
// - file empty
// - happy path

TEST_P(FindOrCreate, find_file_missing)
{
  // File missing. Should create a new file
  llarp::SecretKey key;
  fs::path p = randFilename();
  ASSERT_FALSE(fs::exists(fs::status(p)));

  FileGuard guard(p);

  ASSERT_TRUE(GetParam()(&crypto, p, key));
  ASSERT_TRUE(fs::exists(fs::status(p)));
  ASSERT_FALSE(key.IsZero());
}

TEST_P(FindOrCreate, find_file_empty)
{
  // File empty.
  llarp::SecretKey key;
  fs::path p = randFilename();
  ASSERT_FALSE(fs::exists(fs::status(p)));

  std::fstream f;
  f.open(p.string(), std::ios::out);
  f.close();

  FileGuard guard(p);

  ASSERT_FALSE(GetParam()(&crypto, p, key));
  // Verify we didn't delete an invalid file
  ASSERT_TRUE(fs::exists(fs::status(p)));
}

TEST_P(FindOrCreate, happy_path)
{
  // happy path.
  llarp::SecretKey key;
  fs::path p = randFilename();
  ASSERT_FALSE(fs::exists(fs::status(p)));

  std::ofstream f;
  f.open(p.string(), std::ios::out);
  std::fill_n(std::ostream_iterator< byte_t >(f), key.size(), 0x20);
  f.close();

  FileGuard guard(p);

  ASSERT_TRUE(GetParam()(&crypto, p, key));
  // Verify we didn't delete the file
  ASSERT_TRUE(fs::exists(fs::status(p)));
}

FindOrCreateFunc findOrCreateFunc[] = {llarp_findOrCreateEncryption,
                                       llarp_findOrCreateIdentity};

INSTANTIATE_TEST_CASE_P(TestRouter, FindOrCreate,
                        ::testing::ValuesIn(findOrCreateFunc));
