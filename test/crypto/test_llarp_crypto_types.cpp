#include <crypto/types.hpp>

#include <fstream>
#include <string>

#include <test_util.hpp>
#include <gtest/gtest.h>

// This used to be implied via the headers above *shrug*
#ifdef _WIN32
#include <windows.h>
#endif

struct ToStringData
{
  llarp::PubKey::Data input;
  std::string output;
};

struct PubKeyString : public ::testing::TestWithParam< ToStringData >
{
};

TEST_P(PubKeyString, tostring)
{
  auto d = GetParam();
  llarp::PubKey key(d.input);

  ASSERT_EQ(key.ToString(), d.output);
}

TEST_P(PubKeyString, fromstring)
{
  auto d = GetParam();

  llarp::PubKey key;

  ASSERT_TRUE(key.FromString(d.output));
  ASSERT_EQ(key, llarp::PubKey(d.input));
}

llarp::PubKey::Data empty = {};
llarp::PubKey::Data full  = {{0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                             0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                             0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                             0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff}};

// clang-format off
ToStringData toStringData[] = {
    {empty, "0000000000000000000000000000000000000000000000000000000000000000"},
    {full, "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"},
};
// clang-format on

INSTANTIATE_TEST_SUITE_P(TestCryptoTypes, PubKeyString,
                         ::testing::ValuesIn(toStringData));

// Concerns
// - file missing
// - file empty
// - file too small
// - file too large
// - raw buffer
// - bencoded

struct TestCryptoTypesSecret : public ::testing::Test
{
  std::string filename;
  fs::path p;

  TestCryptoTypesSecret() : filename(llarp::test::randFilename()), p(filename)
  {
  }
};

TEST_F(TestCryptoTypesSecret, secret_key_from_file_missing)
{
  // Verify loading an empty file fails cleanly.
  ASSERT_FALSE(fs::exists(fs::status(p)));

  llarp::SecretKey key;
  ASSERT_FALSE(key.LoadFromFile(filename.c_str()));

  // Verify we didn't create a file
  ASSERT_FALSE(fs::exists(fs::status(p)));
}

TEST_F(TestCryptoTypesSecret, secret_key_from_file_empty)
{
  // Verify loading an empty file fails cleanly.
  ASSERT_FALSE(fs::exists(fs::status(p)));

  // Create empty file
  std::fstream f;
  f.open(filename, std::ios::out | std::ios::binary);
  f.close();

  llarp::test::FileGuard guard(p);

  llarp::SecretKey key;
  ASSERT_FALSE(key.LoadFromFile(filename.c_str()));

  // Verify we didn't delete the file
  ASSERT_TRUE(fs::exists(fs::status(fs::path(filename))));
}

TEST_F(TestCryptoTypesSecret, secret_key_from_file_smaller)
{
  // Verify loading a file which is too small fails cleanly.
  ASSERT_FALSE(fs::exists(fs::status(p)));

  // Create empty file
  std::fstream f;
  f.open(filename, std::ios::out | std::ios::binary);
  std::fill_n(std::ostream_iterator< byte_t >(f), llarp::SecretKey::SIZE / 2,
              0xAA);
  f.close();

  llarp::test::FileGuard guard(p);

  llarp::SecretKey key;
  ASSERT_FALSE(key.LoadFromFile(filename.c_str()));

  // Verify we didn't delete the file
  ASSERT_TRUE(fs::exists(fs::status(fs::path(filename))));
}

TEST_F(TestCryptoTypesSecret, secret_key_from_file_smaller_bencode)
{
  // Verify loading a file which is too small fails cleanly.
  ASSERT_FALSE(fs::exists(fs::status(p)));

  // Create empty file
  std::fstream f;
  f.open(filename, std::ios::out | std::ios::binary);
  f.write("32:", 3);
  std::fill_n(std::ostream_iterator< byte_t >(f), 32, 0xAA);
  f.close();

  llarp::test::FileGuard guard(p);

  llarp::SecretKey key;
  ASSERT_FALSE(key.LoadFromFile(filename.c_str()));

  // Verify we didn't delete the file
  ASSERT_TRUE(fs::exists(fs::status(fs::path(filename))));
}

TEST_F(TestCryptoTypesSecret, secret_key_from_file_smaller_corrupt_bencode)
{
  // Verify loading a file which is too small + corrupt fails cleanly.
  ASSERT_FALSE(fs::exists(fs::status(p)));

  // Create empty file
  std::fstream f;
  f.open(filename, std::ios::out | std::ios::binary);
  f.write("256:", 4);
  std::fill_n(std::ostream_iterator< byte_t >(f), 32, 0xAA);
  f.close();

  llarp::test::FileGuard guard(p);

  llarp::SecretKey key;
  ASSERT_FALSE(key.LoadFromFile(filename.c_str()));

  // Verify we didn't delete the file
  ASSERT_TRUE(fs::exists(fs::status(fs::path(filename))));
}

TEST_F(TestCryptoTypesSecret, secret_key_from_file_larger)
{
  // Verify loading a file which is too large fails cleanly.
  ASSERT_FALSE(fs::exists(fs::status(p)));

  // Create empty file
  std::fstream f;
  f.open(filename, std::ios::out | std::ios::binary);
  std::fill_n(std::ostream_iterator< byte_t >(f), llarp::SecretKey::SIZE * 2,
              0xAA);
  f.close();

  llarp::test::FileGuard guard(p);

  llarp::SecretKey key;
  ASSERT_FALSE(key.LoadFromFile(filename.c_str()));

  // Verify we didn't delete the file
  ASSERT_TRUE(fs::exists(fs::status(fs::path(filename))));
}

TEST_F(TestCryptoTypesSecret, secret_key_from_file_larger_bencode)
{
  // Verify loading a file which is too large fails cleanly.
  ASSERT_FALSE(fs::exists(fs::status(p)));

  // Create empty file
  std::fstream f;
  f.open(filename, std::ios::out | std::ios::binary);
  f.write("256:", 4);
  std::fill_n(std::ostream_iterator< byte_t >(f), 256, 0xAA);
  f.close();

  llarp::test::FileGuard guard(p);

  llarp::SecretKey key;
  ASSERT_FALSE(key.LoadFromFile(filename.c_str()));

  // Verify we didn't delete the file
  ASSERT_TRUE(fs::exists(fs::status(fs::path(filename))));
}

TEST_F(TestCryptoTypesSecret, secret_key_from_file_happy_raw)
{
  // Verify loading a valid raw file succeeds.
  ASSERT_FALSE(fs::exists(fs::status(p)));

  // Create empty file
  std::fstream f;
  f.open(filename, std::ios::out | std::ios::binary);
  std::fill_n(std::ostream_iterator< byte_t >(f), llarp::SecretKey::SIZE, 0xAA);
  f.close();

  llarp::test::FileGuard guard(p);

  llarp::SecretKey key;
  ASSERT_TRUE(key.LoadFromFile(filename.c_str()));

  // Verify we didn't delete the file
  ASSERT_TRUE(fs::exists(fs::status(fs::path(filename))));
}

TEST_F(TestCryptoTypesSecret, secret_key_from_file_happy_bencode)
{
  // Verify loading a valid bencoded file succeeds.
  ASSERT_FALSE(fs::exists(fs::status(p)));

  // Create empty file
  std::fstream f;
  f.open(filename, std::ios::out | std::ios::binary);
  f.write("64:", 4);
  std::fill_n(std::ostream_iterator< byte_t >(f), llarp::SecretKey::SIZE, 0xAA);
  f.close();

  llarp::test::FileGuard guard(p);

  llarp::SecretKey key;
  ASSERT_TRUE(key.LoadFromFile(filename.c_str()));

  // Verify we didn't delete the file
  ASSERT_TRUE(fs::exists(fs::status(fs::path(filename))));
}

// Save to file

// Concerns
// - file not writeable
// - happy path

// Win32: check for root/admin/elevation privileges
#ifdef _WIN32
BOOL
IsRunAsAdmin()
{
  BOOL fIsRunAsAdmin        = FALSE;
  DWORD dwError             = ERROR_SUCCESS;
  PSID pAdministratorsGroup = NULL;

  // Allocate and initialize a SID of the administrators group.
  SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
  if(!AllocateAndInitializeSid(&NtAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID,
                               DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0,
                               &pAdministratorsGroup))
  {
    dwError = GetLastError();
    goto Cleanup;
  }

  // Determine whether the SID of administrators group is enabled in
  // the primary access token of the process.
  if(!CheckTokenMembership(NULL, pAdministratorsGroup, &fIsRunAsAdmin))
  {
    dwError = GetLastError();
    goto Cleanup;
  }

Cleanup:
  // Centralized cleanup for all allocated resources.
  if(pAdministratorsGroup)
  {
    FreeSid(pAdministratorsGroup);
    pAdministratorsGroup = NULL;
  }

  // Throw the error if something failed in the function.
  if(ERROR_SUCCESS != dwError)
  {
    throw dwError;
  }

  return fIsRunAsAdmin;
}
#endif

TEST_F(TestCryptoTypesSecret, secret_key_to_missing_file)
{
  // Verify writing to an unwritable file fails.
  // Assume we're not running as root, so can't write to [C:]/
  // if we are root just skip this test
#ifndef _WIN32
  if(getuid() == 0)
    return;
#else
  if(IsRunAsAdmin())
    return;
#endif
  filename = "/" + filename;
  p        = filename;
  ASSERT_FALSE(fs::exists(fs::status(p)));

  llarp::test::FileGuard guard(p);

  llarp::SecretKey key;
  ASSERT_FALSE(key.SaveToFile(filename.c_str()));

  // Verify we didn't create the file
  ASSERT_FALSE(fs::exists(fs::status(fs::path(filename))));
}

TEST_F(TestCryptoTypesSecret, secret_key_to_file)
{
  ASSERT_FALSE(fs::exists(fs::status(p)));

  llarp::test::FileGuard guard(p);

  llarp::SecretKey key;
  key.Randomize();
  ASSERT_TRUE(key.SaveToFile(filename.c_str()));

  // Verify we created the file
  ASSERT_TRUE(fs::exists(fs::status(fs::path(filename))));

  llarp::SecretKey other;
  other.LoadFromFile(filename.c_str());

  ASSERT_EQ(other, key);
}
