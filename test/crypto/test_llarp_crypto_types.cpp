#include <crypto/types.hpp>

#include <fstream>
#include <string>

#include <test_util.hpp>
#include <catch2/catch.hpp>

// This used to be implied via the headers above *shrug*
#ifdef _WIN32
#include <windows.h>
#endif

struct ToStringData
{
  llarp::PubKey::Data input;
  std::string output;
};

llarp::PubKey::Data empty = {};
llarp::PubKey::Data full = {{0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                             0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                             0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff}};

// clang-format off
std::vector<ToStringData> toStringData{
    {empty, "0000000000000000000000000000000000000000000000000000000000000000"},
    {full, "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"},
};
// clang-format on

TEST_CASE("PubKey-string conversion")
{
  auto d = GENERATE(from_range(toStringData));

  SECTION("To string")
  {
    llarp::PubKey key(d.input);

    REQUIRE(key.ToString() == d.output);
  }

  SECTION("From string")
  {
    llarp::PubKey key;

    REQUIRE(key.FromString(d.output));
    REQUIRE(key == llarp::PubKey(d.input));
  }
}

// Concerns
// - file missing
// - file empty
// - file too small
// - file too large
// - raw buffer
// - bencoded

struct TestCryptoTypesSecret
{
  std::string filename;
  fs::path p;

  TestCryptoTypesSecret() : filename(llarp::test::randFilename()), p(filename)
  {}
};

TEST_CASE_METHOD(TestCryptoTypesSecret, "secret_key_from_file_missing")
{
  // Verify loading an empty file fails cleanly.
  REQUIRE_FALSE(fs::exists(fs::status(p)));

  llarp::SecretKey key;
  REQUIRE_FALSE(key.LoadFromFile(filename.c_str()));

  // Verify we didn't create a file
  REQUIRE_FALSE(fs::exists(fs::status(p)));
}

TEST_CASE_METHOD(TestCryptoTypesSecret, "secret_key_from_file_empty")
{
  // Verify loading an empty file fails cleanly.
  REQUIRE_FALSE(fs::exists(fs::status(p)));

  // Create empty file
  std::fstream f;
  f.open(filename, std::ios::out | std::ios::binary);
  f.close();

  llarp::test::FileGuard guard(p);

  llarp::SecretKey key;
  REQUIRE_FALSE(key.LoadFromFile(filename.c_str()));

  // Verify we didn't delete the file
  REQUIRE(fs::exists(fs::status(fs::path(filename))));
}

TEST_CASE_METHOD(TestCryptoTypesSecret, "secret_key_from_file_smaller")
{
  // Verify loading a file which is too small fails cleanly.
  REQUIRE_FALSE(fs::exists(fs::status(p)));

  // Create empty file
  std::fstream f;
  f.open(filename, std::ios::out | std::ios::binary);
  std::fill_n(std::ostream_iterator<byte_t>(f), llarp::SecretKey::SIZE / 2, 0xAA);
  f.close();

  llarp::test::FileGuard guard(p);

  llarp::SecretKey key;
  REQUIRE_FALSE(key.LoadFromFile(filename.c_str()));

  // Verify we didn't delete the file
  REQUIRE(fs::exists(fs::status(fs::path(filename))));
}

TEST_CASE_METHOD(TestCryptoTypesSecret, "secret_key_from_file_smaller_bencode")
{
  // Verify loading a file which is too small fails cleanly.
  REQUIRE_FALSE(fs::exists(fs::status(p)));

  // Create empty file
  std::fstream f;
  f.open(filename, std::ios::out | std::ios::binary);
  f.write("32:", 3);
  std::fill_n(std::ostream_iterator<byte_t>(f), 32, 0xAA);
  f.close();

  llarp::test::FileGuard guard(p);

  llarp::SecretKey key;
  REQUIRE_FALSE(key.LoadFromFile(filename.c_str()));

  // Verify we didn't delete the file
  REQUIRE(fs::exists(fs::status(fs::path(filename))));
}

TEST_CASE_METHOD(TestCryptoTypesSecret, "secret_key_from_file_smaller_corrupt_bencode")
{
  // Verify loading a file which is too small + corrupt fails cleanly.
  REQUIRE_FALSE(fs::exists(fs::status(p)));

  // Create empty file
  std::fstream f;
  f.open(filename, std::ios::out | std::ios::binary);
  f.write("256:", 4);
  std::fill_n(std::ostream_iterator<byte_t>(f), 32, 0xAA);
  f.close();

  llarp::test::FileGuard guard(p);

  llarp::SecretKey key;
  REQUIRE_FALSE(key.LoadFromFile(filename.c_str()));

  // Verify we didn't delete the file
  REQUIRE(fs::exists(fs::status(fs::path(filename))));
}

TEST_CASE_METHOD(TestCryptoTypesSecret, "secret_key_from_file_larger")
{
  // Verify loading a file which is too large fails cleanly.
  REQUIRE_FALSE(fs::exists(fs::status(p)));

  // Create empty file
  std::fstream f;
  f.open(filename, std::ios::out | std::ios::binary);
  std::fill_n(std::ostream_iterator<byte_t>(f), llarp::SecretKey::SIZE * 2, 0xAA);
  f.close();

  llarp::test::FileGuard guard(p);

  llarp::SecretKey key;
  REQUIRE_FALSE(key.LoadFromFile(filename.c_str()));

  // Verify we didn't delete the file
  REQUIRE(fs::exists(fs::status(fs::path(filename))));
}

TEST_CASE_METHOD(TestCryptoTypesSecret, "secret_key_from_file_larger_bencode")
{
  // Verify loading a file which is too large fails cleanly.
  REQUIRE_FALSE(fs::exists(fs::status(p)));

  // Create empty file
  std::fstream f;
  f.open(filename, std::ios::out | std::ios::binary);
  f.write("256:", 4);
  std::fill_n(std::ostream_iterator<byte_t>(f), 256, 0xAA);
  f.close();

  llarp::test::FileGuard guard(p);

  llarp::SecretKey key;
  REQUIRE_FALSE(key.LoadFromFile(filename.c_str()));

  // Verify we didn't delete the file
  REQUIRE(fs::exists(fs::status(fs::path(filename))));
}

TEST_CASE_METHOD(TestCryptoTypesSecret, "secret_key_from_file_happy_raw")
{
  // Verify loading a valid raw file succeeds.
  REQUIRE_FALSE(fs::exists(fs::status(p)));

  // Create empty file
  std::fstream f;
  f.open(filename, std::ios::out | std::ios::binary);
  std::fill_n(std::ostream_iterator<byte_t>(f), llarp::SecretKey::SIZE, 0xAA);
  f.close();

  llarp::test::FileGuard guard(p);

  llarp::SecretKey key;
  REQUIRE(key.LoadFromFile(filename.c_str()));

  // Verify we didn't delete the file
  REQUIRE(fs::exists(fs::status(fs::path(filename))));
}

TEST_CASE_METHOD(TestCryptoTypesSecret, "secret_key_from_file_happy_bencode")
{
  // Verify loading a valid bencoded file succeeds.
  REQUIRE_FALSE(fs::exists(fs::status(p)));

  // Create empty file
  std::fstream f;
  f.open(filename, std::ios::out | std::ios::binary);
  f.write("64:", 4);
  std::fill_n(std::ostream_iterator<byte_t>(f), llarp::SecretKey::SIZE, 0xAA);
  f.close();

  llarp::test::FileGuard guard(p);

  llarp::SecretKey key;
  REQUIRE(key.LoadFromFile(filename.c_str()));

  // Verify we didn't delete the file
  REQUIRE(fs::exists(fs::status(fs::path(filename))));
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
  BOOL fIsRunAsAdmin = FALSE;
  DWORD dwError = ERROR_SUCCESS;
  PSID pAdministratorsGroup = NULL;

  // Allocate and initialize a SID of the administrators group.
  SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
  if (!AllocateAndInitializeSid(
          &NtAuthority,
          2,
          SECURITY_BUILTIN_DOMAIN_RID,
          DOMAIN_ALIAS_RID_ADMINS,
          0,
          0,
          0,
          0,
          0,
          0,
          &pAdministratorsGroup))
  {
    dwError = GetLastError();
    goto Cleanup;
  }

  // Determine whether the SID of administrators group is enabled in
  // the primary access token of the process.
  if (!CheckTokenMembership(NULL, pAdministratorsGroup, &fIsRunAsAdmin))
  {
    dwError = GetLastError();
    goto Cleanup;
  }

Cleanup:
  // Centralized cleanup for all allocated resources.
  if (pAdministratorsGroup)
  {
    FreeSid(pAdministratorsGroup);
    pAdministratorsGroup = NULL;
  }

  // Throw the error if something failed in the function.
  if (ERROR_SUCCESS != dwError)
  {
    throw dwError;
  }

  return fIsRunAsAdmin;
}
#endif

TEST_CASE_METHOD(TestCryptoTypesSecret, "secret_key_to_missing_file")
{
  // Verify writing to an unwritable file fails.
  // Assume we're not running as root, so can't write to [C:]/
  // if we are root just skip this test
#ifndef _WIN32
  if (getuid() == 0)
    return;
#else
  if (IsRunAsAdmin())
    return;
#endif
  filename = "/" + filename;
  p = filename;
  REQUIRE_FALSE(fs::exists(fs::status(p)));

  llarp::test::FileGuard guard(p);

  llarp::SecretKey key;
  REQUIRE_FALSE(key.SaveToFile(filename.c_str()));

  // Verify we didn't create the file
  REQUIRE_FALSE(fs::exists(fs::status(fs::path(filename))));
}

TEST_CASE_METHOD(TestCryptoTypesSecret, "secret_key_to_file")
{
  REQUIRE_FALSE(fs::exists(fs::status(p)));

  llarp::test::FileGuard guard(p);

  llarp::SecretKey key;
  key.Randomize();
  REQUIRE(key.SaveToFile(filename.c_str()));

  // Verify we created the file
  REQUIRE(fs::exists(fs::status(fs::path(filename))));

  llarp::SecretKey other;
  other.LoadFromFile(filename.c_str());

  REQUIRE(other == key);
}
