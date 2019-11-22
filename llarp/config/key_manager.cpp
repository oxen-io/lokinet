#include <config/key_manager.hpp>

#include <util/logging/logger.hpp>

namespace llarp
{
  KeyManager::KeyManager(const llarp::Config& config)
    : m_rcPath(config.router.ourRcFile())
    , m_idKeyPath(config.router.identKeyfile())
    , m_encKeyPath(config.router.encryptionKeyfile())
    , m_transportKeyPath(config.router.transportKeyfile())
  {
  }

  bool
  KeyManager::initializeFromDisk(bool genIfAbsent)
  {

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

        // TODO: generate files
      }
    }

    // TODO: load files

    return true;
  }

  bool
  KeyManager::getIdentityKey(llarp::SecretKey &key) const
  {
    return true;
  }

  bool
  KeyManager::getEncryptionKey(llarp::SecretKey &key) const
  {
    return true;
  }

  bool
  KeyManager::getTransportKey(llarp::SecretKey &key) const
  {
    return true;
  }

  bool
  KeyManager::getRouterContact(llarp::RouterContact& rc) const
  {
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
  }

}  // namespace llarp
