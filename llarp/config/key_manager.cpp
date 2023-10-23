#include "key_manager.hpp"

#include <system_error>
#include <llarp/util/logging.hpp>
#include "config.hpp"
#include <llarp/crypto/crypto.hpp>
#include <llarp/crypto/types.hpp>

namespace llarp
{
  KeyManager::KeyManager() : m_initialized(false), m_needBackup(false)
  {}

  bool
  KeyManager::initialize(const llarp::Config& config, bool genIfAbsent, bool isSNode)
  {
    if (m_initialized)
      return false;

    if (not isSNode)
    {
      crypto::identity_keygen(identityKey);
      crypto::encryption_keygen(encryptionKey);
      crypto::encryption_keygen(transportKey);
      return true;
    }

    const fs::path root = config.router.m_dataDir;

    // utility function to assign a path, using the specified config parameter if present and
    // falling back to root / defaultName if not
    auto deriveFile = [&](const std::string& defaultName, const std::string& option) {
      if (option.empty())
      {
        return root / defaultName;
      }
      else
      {
        fs::path file(option);
        if (not file.is_absolute())
          file = root / file;

        return file;
      }
    };

    m_rcPath = deriveFile(our_rc_filename, config.router.m_routerContactFile);
    m_idKeyPath = deriveFile(our_identity_filename, config.router.m_identityKeyFile);
    m_encKeyPath = deriveFile(our_enc_key_filename, config.router.m_encryptionKeyFile);
    m_transportKeyPath = deriveFile(our_transport_key_filename, config.router.m_transportKeyFile);

    RouterContact rc;
    bool exists = rc.Read(m_rcPath);
    if (not exists and not genIfAbsent)
    {
      LogError("Could not read RouterContact at path ", m_rcPath);
      return false;
    }

    // we need to back up keys if our self.signed doesn't appear to have a
    // valid signature
    m_needBackup = (isSNode and not rc.VerifySignature());

    // if our RC file can't be verified, assume it is out of date (e.g. uses
    // older encryption) and needs to be regenerated. before doing so, backup
    // files that will be overwritten
    if (exists and m_needBackup)
    {
      if (!genIfAbsent)
      {
        LogError("Our RouterContact ", m_rcPath, " is invalid or out of date");
        return false;
      }
      else
      {
        LogWarn(
            "Our RouterContact ",
            m_rcPath,
            " seems out of date, backing up and regenerating private keys");

        if (!backupKeyFilesByMoving())
        {
          LogError(
              "Could not mv some key files, please ensure key files"
              " are backed up if needed and remove");
          return false;
        }
      }
    }

    if (not config.router.m_isRelay)
    {
      // load identity key or create if needed
      auto identityKeygen = [](llarp::SecretKey& key) {
        // TODO: handle generating from service node seed
        llarp::crypto::identity_keygen(key);
      };
      if (not loadOrCreateKey(m_idKeyPath, identityKey, identityKeygen))
        return false;
    }

    // load encryption key
    auto encryptionKeygen = [](llarp::SecretKey& key) { llarp::crypto::encryption_keygen(key); };
    if (not loadOrCreateKey(m_encKeyPath, encryptionKey, encryptionKeygen))
      return false;

    // TODO: transport key (currently done in LinkLayer)
    auto transportKeygen = [](llarp::SecretKey& key) {
      key.Zero();
      crypto::encryption_keygen(key);
    };
    if (not loadOrCreateKey(m_transportKeyPath, transportKey, transportKeygen))
      return false;

    m_initialized = true;
    return true;
  }

  bool
  KeyManager::backupFileByMoving(const fs::path& filepath)
  {
    auto findFreeBackupFilename = [](const fs::path& filepath) {
      for (int i = 0; i < 9; i++)
      {
        std::string ext("." + std::to_string(i) + ".bak");
        fs::path newPath = filepath;
        newPath += ext;

        if (not fs::exists(newPath))
          return newPath;
      }
      return fs::path();
    };

    std::error_code ec;
    bool exists = fs::exists(filepath, ec);
    if (ec)
    {
      LogError("Could not determine status of file ", filepath, ": ", ec.message());
      return false;
    }

    if (not exists)
    {
      LogInfo("File ", filepath, " doesn't exist; no backup needed");
      return true;
    }

    fs::path newFilepath = findFreeBackupFilename(filepath);
    if (newFilepath.empty())
    {
      LogWarn("Could not find an appropriate backup filename for", filepath);
      return false;
    }

    LogInfo("Backing up (moving) key file ", filepath, " to ", newFilepath, "...");

    fs::rename(filepath, newFilepath, ec);
    if (ec)
    {
      LogError("Failed to move key file ", ec.message());
      return false;
    }

    return true;
  }

  bool
  KeyManager::backupKeyFilesByMoving() const
  {
    std::vector<fs::path> files = {m_rcPath, m_idKeyPath, m_encKeyPath, m_transportKeyPath};

    for (auto& filepath : files)
    {
      if (not backupFileByMoving(filepath))
        return false;
    }

    return true;
  }

  bool
  KeyManager::loadOrCreateKey(
      fs::path path, llarp::SecretKey& key, std::function<void(llarp::SecretKey& key)> keygen)
  {
    if (not fs::exists(path))
    {
      LogInfo("Generating new key", path);
      keygen(key);

      if (!key.SaveToFile(path))
      {
        LogError("Failed to save new key");
        return false;
      }
    }
    LogDebug("Loading key from file ", path);
    return key.LoadFromFile(path);
  }

}  // namespace llarp
