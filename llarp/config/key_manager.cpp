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
    // TODO:
    // 1) start with the RouterContact file. We can detect the version from
    //    this and decide whether or not the existing keys need updating.
    // 2) Backup existing files if necessary
    // 3) Write new files if necessary
    // 4) Load files to be obtained later
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

}  // namespace llarp
