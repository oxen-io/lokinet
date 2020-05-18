#include <config/key_manager.hpp>

#include <system_error>
#include <util/logging/logger.hpp>
#include "config/config.hpp"
#include "crypto/crypto.hpp"
#include "crypto/types.hpp"

#ifdef HAVE_CURL
#include <curl/curl.h>
#endif

/// curl callback
static size_t
curl_RecvIdentKey(char* ptr, size_t, size_t nmemb, void* userdata)
{
  for (size_t idx = 0; idx < nmemb; idx++)
    static_cast<std::vector<char>*>(userdata)->push_back(ptr[idx]);
  return nmemb;
}

namespace llarp
{
  KeyManager::KeyManager() : m_initialized(false), m_needBackup(false)
  {
  }

  bool
  KeyManager::initialize(const llarp::Config& config, bool genIfAbsent)
  {
    if (m_initialized)
      return false;

    fs::path root = config.router.m_dataDir;

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
          throw std::runtime_error(stringify("override for ", defaultName, " cannot be relative"));

        return file;
      }
    };

    m_rcPath = deriveFile(our_rc_filename, config.router.m_routerContactFile);
    m_idKeyPath = deriveFile(our_identity_filename, config.router.m_identityKeyFile);
    m_encKeyPath = deriveFile(our_enc_key_filename, config.router.m_encryptionKeyFile);
    m_transportKeyPath = deriveFile(our_transport_key_filename, config.router.m_transportKeyFile);

    m_usingLokid = config.lokid.whitelistRouters;
    m_lokidRPCAddr = config.lokid.lokidRPCAddr;
    m_lokidRPCUser = config.lokid.lokidRPCUser;
    m_lokidRPCPassword = config.lokid.lokidRPCPassword;

    RouterContact rc;
    bool exists = rc.Read(m_rcPath.c_str());
    if (not exists and not genIfAbsent)
    {
      LogError("Could not read RouterContact at path ", m_rcPath);
      return false;
    }

    // we need to back up keys if our self.signed doesn't appear to have a
    // valid signature
    m_needBackup = (not rc.VerifySignature());

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

    if (not m_usingLokid)
    {
      // load identity key or create if needed
      auto identityKeygen = [](llarp::SecretKey& key) {
        // TODO: handle generating from service node seed
        llarp::CryptoManager::instance()->identity_keygen(key);
      };
      if (not loadOrCreateKey(m_idKeyPath, identityKey, identityKeygen))
        return false;
    }
    else
    {
      if (not loadIdentityFromLokid())
        return false;
    }

    // load encryption key
    auto encryptionKeygen = [](llarp::SecretKey& key) {
      llarp::CryptoManager::instance()->encryption_keygen(key);
    };
    if (not loadOrCreateKey(m_encKeyPath, encryptionKey, encryptionKeygen))
      return false;

    // TODO: transport key (currently done in LinkLayer)
    auto transportKeygen = [](llarp::SecretKey& key) {
      key.Zero();
      CryptoManager::instance()->encryption_keygen(key);
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
    std::vector<std::string> files = {m_rcPath, m_idKeyPath, m_encKeyPath, m_transportKeyPath};

    for (auto& filepath : files)
    {
      if (not backupFileByMoving(filepath))
        return false;
    }

    return true;
  }

  bool
  KeyManager::loadOrCreateKey(
      const fs::path& filepath,
      llarp::SecretKey& key,
      std::function<void(llarp::SecretKey& key)> keygen)
  {
    fs::path path(filepath);
    std::error_code ec;
    if (!fs::exists(path, ec))
    {
      if (ec)
      {
        LogError("Error checking key", filepath, ec.message());
        return false;
      }

      LogInfo("Generating new key", filepath);
      keygen(key);

      if (!key.SaveToFile(filepath.c_str()))
      {
        LogError("Failed to save new key");
        return false;
      }
    }

    LogDebug("Loading key from file ", filepath);
    return key.LoadFromFile(filepath.c_str());
  }

  bool
  KeyManager::loadIdentityFromLokid()
  {
#ifndef HAVE_CURL
    LogError("this lokinet was not built with service node mode support");
    return false;
#else
    CURL* curl = curl_easy_init();
    if (curl)
    {
      bool ret = false;
      std::stringstream ss;
      ss << "http://" << m_lokidRPCAddr << "/json_rpc";
      const auto url = ss.str();
      curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
      curl_easy_setopt(curl, CURLOPT_HTTPAUTH, (long)CURLAUTH_ANY);
      const auto auth = m_lokidRPCUser + ":" + m_lokidRPCPassword;
      curl_easy_setopt(curl, CURLOPT_USERPWD, auth.c_str());
      curl_slist* list = nullptr;
      list = curl_slist_append(list, "Content-Type: application/json");
      curl_easy_setopt(curl, CURLOPT_HTTPHEADER, list);

      nlohmann::json request = {
          {"id", "0"}, {"jsonrpc", "2.0"}, {"method", "get_service_node_privkey"}};
      const auto data = request.dump();
      std::vector<char> resp;

      curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, data.size());
      curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.c_str());
      curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp);
      curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &curl_RecvIdentKey);

      resp.clear();
      LogInfo("Getting Identity Keys from lokid...");
      if (curl_easy_perform(curl) == CURLE_OK)
      {
        try
        {
          auto j = nlohmann::json::parse(resp);
          if (not j.is_object())
            return false;

          const auto itr = j.find("result");
          if (itr == j.end())
            return false;
          if (not itr->is_object())
            return false;
          const auto k = (*itr)["service_node_ed25519_privkey"].get<std::string>();
          if (k.size() != (identityKey.size() * 2))
          {
            if (k.empty())
            {
              LogError("lokid gave no identity key");
            }
            else
            {
              LogError("lokid gave invalid identity key");
            }
            return false;
          }
          if (not HexDecode(k.c_str(), identityKey.data(), identityKey.size()))
            return false;
          if (CryptoManager::instance()->check_identity_privkey(identityKey))
          {
            ret = true;
          }
          else
          {
            LogError("lokid gave bogus identity key");
          }
        }
        catch (nlohmann::json::exception& ex)
        {
          LogError("Bad response from lokid: ", ex.what());
        }
      }
      else
      {
        LogError("failed to get identity keys");
      }
      if (ret)
      {
        LogInfo("Got Identity Keys from lokid: ", RouterID(seckey_topublic(identityKey)));
      }
      curl_easy_cleanup(curl);
      curl_slist_free_all(list);
      return ret;
    }
    else
    {
      LogError("failed to init curl");
      return false;
    }
#endif
  }

}  // namespace llarp
