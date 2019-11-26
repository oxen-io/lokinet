#include <config/key_manager.hpp>

#include <system_error>
#include <util/logging/logger.hpp>
#include "config/config.hpp"
#include "crypto/crypto.hpp"
#include "crypto/types.hpp"

namespace llarp
{
  KeyManager::KeyManager()
    : m_initialized(false)
  {
  }

  bool
  KeyManager::initializeFromDisk(const llarp::Config& config, bool genIfAbsent)
  {
    if (m_initialized)
      return false;

    m_rcPath = config.router.ourRcFile();
    m_idKeyPath = config.router.identKeyfile();
    m_encKeyPath = config.router.encryptionKeyfile();
    m_transportKeyPath = config.router.transportKeyfile();

    RouterContact rc;
    if (!rc.Read(m_rcPath.c_str()))
    {
        LogWarn("Could not read RouterContact at path ", m_rcPath);
        return false;
    }

    if (rc.keyfileVersion < LLARP_KEYFILE_VERSION) {
      if (! genIfAbsent) {
        LogError("Our RouterContact", m_rcPath, "is out of date");
      } else {
        LogWarn("Our RouterContact", m_rcPath,
            "is out of date, backing up and regenerating private keys");

        if (! backupKeyFilesByMoving()) {
          LogError("Could not mv some key files, please ensure key files"
              " are backed up if needed and remove");
          return false;
        }


        // load identity key or create if needed
        auto identityKeygen = [](llarp::SecretKey& key)
        {
          // TODO: handle generating from service node seed
          llarp::CryptoManager::instance()->identity_keygen(key);
        };
        if (not loadOrCreateKey(m_idKeyPath, m_idKey, identityKeygen))
          return false;

        // load encryption key
        auto encryptionKeygen = [](llarp::SecretKey& key)
        {
          llarp::CryptoManager::instance()->encryption_keygen(key);
        };
        if (not loadOrCreateKey(m_encKeyPath, m_encKey, encryptionKeygen))
          return false;

        // TODO: transport key (currently done in LinkLayer)

      }
    }

    m_initialized = true;
    return true;
  }

  bool
  KeyManager::getIdentityKey(llarp::SecretKey &key) const
  {
    if (! m_initialized)
      return false;

    key = m_idKey;
    return true;
  }

  bool
  KeyManager::getEncryptionKey(llarp::SecretKey &key) const
  {
    if (! m_initialized)
      return false;

    key = m_encKey;
    return true;
  }

  bool
  KeyManager::getTransportKey(llarp::SecretKey &key) const
  {
    if (! m_initialized)
      return false;

    key = m_transportKey;
    return true;
  }

  bool
  KeyManager::getRouterContact(llarp::RouterContact& rc) const
  {
    if (! m_initialized)
      return false;

    rc = m_rc;
    return true;
  }

  bool
  KeyManager::backupKeyFilesByMoving() const
  {
    auto findFreeBackupFilename = [](const fs::path& filepath) {
      for (int i=0; i<9; i++)
      {
        std::string ext("." + std::to_string(i) + ".bak");
        fs::path newPath = filepath;
        newPath += ext;

        if (not fs::exists(newPath))
          return newPath;
      }
      return fs::path();
    };

    std::vector<std::string> files = {
      m_rcPath,
      m_idKeyPath,
      m_encKeyPath,
      m_transportKeyPath
    };

    for (auto& filepath : files)
    {
      fs::path newFilepath = findFreeBackupFilename(filepath);
      if (newFilepath.empty())
      {
        LogWarn("Could not find an appropriate backup filename for", filepath);
        return false;
      }

      LogInfo("Backing up (moving) key file", filepath, "to", newFilepath, "...");

      std::error_code ec;
      fs::rename(filepath, newFilepath, ec);
      if (ec) {
        LogError("Failed to move key file", ec.message());
        return false;
      }
    }

    return true;
  }

  bool
  KeyManager::loadOrCreateKey(
      const std::string& filepath,
      llarp::SecretKey key,
      std::function<void(llarp::SecretKey&  key)> keygen)
  {
    fs::path path(filepath);
    std::error_code ec;
    if (! fs::exists(path, ec))
    {
      if (ec)
      {
        LogError("Error checking key", filepath, ec.message());
        return false;
      }

      LogInfo("Generating new key", filepath);
      keygen(key);

      if (! key.SaveToFile(filepath.c_str()))
      {
        LogError("Failed to save new key");
        return false;
      }
    }

    return key.LoadFromFile(filepath.c_str());
  }

}  // namespace llarp
