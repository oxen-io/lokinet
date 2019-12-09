#include <config/key_manager.hpp>

#include <crypto/crypto.hpp>
#include <crypto/crypto_libsodium.hpp>
#include <llarp_test.hpp>

#include <functional>
#include <random>

#include <string>
#include <test_util.hpp>
#include <gtest/gtest.h>

using namespace ::llarp;
using namespace ::testing;

static constexpr auto rcFile = "rc.signed";
static constexpr auto encFile = "encryption.key";
static constexpr auto transportFile = "transport.key";
static constexpr auto identFile = "identity.key";

struct KeyManagerTest : public test::LlarpTest< llarp::sodium::CryptoLibSodium >
{
  // paranoid file guards for anything KeyManager might touch
  test::FileGuard m_rcFileGuard;
  test::FileGuard m_encFileGuard;
  test::FileGuard m_transportFileGuard;
  test::FileGuard m_identFileGuard;

  KeyManagerTest()
      : m_rcFileGuard(rcFile)
      , m_encFileGuard(encFile)
      , m_transportFileGuard(transportFile)
      , m_identFileGuard(identFile)
  {
  }

  /// generate a valid "rc.signed" file
  bool
  generateRcFile()
  {
    RouterContact rc;
    return rc.Write(rcFile);
  }
};

TEST_F(KeyManagerTest, TestBackupFileByMoving_MovesExistingFiles)
{
  fs::path p = test::randFilename();
  ASSERT_FALSE(fs::exists(p));

  // touch file
  std::fstream f;
  f.open(p.string(), std::ios::out);
  f.close();

  KeyManager::backupFileByMoving(p.string());

  ASSERT_FALSE(fs::exists(p));

  fs::path moved = p.string() + ".0.bak";

  ASSERT_TRUE(fs::exists(moved));

  test::FileGuard guard(moved);
};

TEST_F(KeyManagerTest, TestBackupFileByMoving_DoesntTouchNonExistentFiles)
{
  fs::path p = test::randFilename();
  ASSERT_FALSE(fs::exists(p));

  KeyManager::backupFileByMoving(p.string());

  ASSERT_FALSE(fs::exists(p));

  fs::path moved = p.string() + ".0.bak";

  ASSERT_FALSE(fs::exists(moved));
}

TEST_F(KeyManagerTest, TestBackupFileByMoving_FailsIfBackupNamesAreExausted)
{
  fs::path base = test::randFilename();
  ASSERT_FALSE(fs::exists(base));

  // touch file
  {
    std::fstream f;
    f.open(base.string(), std::ios::out);
    f.close();
  }

  test::FileGuard guard(base);

  constexpr uint32_t numBackupNames = 9;
  std::vector<test::FileGuard> guards;
  guards.reserve(numBackupNames);

  // generate backup files foo.0.bak through foo.9.bak
  for (uint32_t i=0; i<numBackupNames; ++i)
  {
    fs::path p = base.string() +"."+ std::to_string(i) +".bak";

    std::fstream f;
    f.open(p.string(), std::ios::out);
    f.close();

    guards.emplace_back(p);

    ASSERT_TRUE(fs::exists(p));
  }

  ASSERT_FALSE(KeyManager::backupFileByMoving(base.string()));

};

TEST_F(KeyManagerTest, EnsureDefaultConfNames)
{
  llarp::Config conf;

  // the default config filenames will suffice, this exists as sanity check to
  // protect against the assumptions made below
  ASSERT_TRUE(conf.router.ourRcFile() == rcFile);
  ASSERT_TRUE(conf.router.encryptionKeyfile() == encFile);
  ASSERT_TRUE(conf.router.transportKeyfile() == transportFile);
  ASSERT_TRUE(conf.router.identKeyfile() == identFile);
}

TEST_F(KeyManagerTest, TestInitialize_MakesKeyfiles)
{
  llarp::Config conf;

  KeyManager keyManager;
  ASSERT_TRUE(keyManager.initialize(conf, true));

  // KeyManager doesn't generate RC file, but should generate others
  ASSERT_FALSE(fs::exists(rcFile));

  ASSERT_TRUE(fs::exists(encFile));
  ASSERT_TRUE(fs::exists(transportFile));
  ASSERT_TRUE(fs::exists(identFile));
}

TEST_F(KeyManagerTest, TestInitialize_RespectsGenFlag)
{
  llarp::Config conf;

  KeyManager keyManager;
  ASSERT_FALSE(keyManager.initialize(conf, false));

  // KeyManager shouldn't have touched any files without (genIfAbsent == true)
  ASSERT_FALSE(fs::exists(rcFile));
  ASSERT_FALSE(fs::exists(encFile));
  ASSERT_FALSE(fs::exists(transportFile));
  ASSERT_FALSE(fs::exists(identFile));
}

TEST_F(KeyManagerTest, TestInitialize_DetectsBadRcFile)
{
  llarp::Config conf;

  std::fstream f;
  f.open(rcFile, std::ios::out);
  f << "bad_rc_file";
  f.close();

  KeyManager keyManager;
  ASSERT_TRUE(keyManager.initialize(conf, true));
  ASSERT_TRUE(keyManager.needBackup());

  ASSERT_TRUE(fs::exists(encFile));
  ASSERT_TRUE(fs::exists(transportFile));
  ASSERT_TRUE(fs::exists(identFile));

  // test that keys are sane
  SecretKey key;

  key.Zero();
  ASSERT_TRUE(key.LoadFromFile(encFile));
  ASSERT_FALSE(key.IsZero());

  key.Zero();
  ASSERT_TRUE(key.LoadFromFile(transportFile));
  ASSERT_FALSE(key.IsZero());

  key.Zero();
  ASSERT_TRUE(key.LoadFromFile(identFile));
  ASSERT_FALSE(key.IsZero());
}

