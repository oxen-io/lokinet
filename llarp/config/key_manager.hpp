#ifndef LLARP_KEY_MANAGER_HPP
#define LLARP_KEY_MANAGER_HPP

#include <atomic>
#include <config/config.hpp>
#include <crypto/types.hpp>
#include <router_contact.hpp>

namespace llarp
{

  /// KeyManager manages the cryptographic keys stored on disk for the local node.
  /// This includes private keys as well as the self-signed router contact file
  /// (e.g. "self.signed").
  ///
  /// Keys are either read from disk if they exist and are valid (see below) or are
  /// generated and written to disk.
  /// 
  /// In addition, the KeyManager detects when the keys obsolete (e.g. as a result
  /// of a software upgrade) and backs up existing keys before writing out new ones.

  struct KeyManager {

    /// Constructor
    KeyManager();

    /// Initializes from disk. This reads enough from disk to understand the current
    /// state of the stored keys.
    ///
    /// NOTE: Must be called prior to obtaining any keys.
    ///
    /// @param config should be a prepared config object
    /// @param genIfAbsent determines whether or not we will create files if they
    ///        do not exist.
    /// @return true on success, false otherwise
    bool
    initializeFromDisk(const llarp::Config& config, bool genIfAbsent);

    /// Obtain the identity key (e.g. ~/.lokinet/identity.private)
    ///
    /// @return a reference to the identity key
    const llarp::SecretKey&
    getIdentityKey() const;

    /// Obtain the encryption key (e.g. ~/.lokinet/encryption.private)
    ///
    /// @return a reference to the encryption key
    const llarp::SecretKey&
    getEncryptionKey() const;

    /// Obtain the transport key (e.g. ~/.lokinet/transport.private)
    ///
    /// @return a reference to the transport key
    const llarp::SecretKey&
    getTransportKey() const;

    /// Obtain the self-signed RouterContact
    ///
    /// @param rc (out) will be modified to contian the RouterContact
    /// @return true on success, false otherwise
    bool
    getRouterContact(llarp::RouterContact& rc) const;

  private:

    std::string m_rcPath;
    std::string m_idKeyPath;
    std::string m_encKeyPath;
    std::string m_transportKeyPath;
    std::atomic_bool m_initialized;

    llarp::RouterContact m_rc;
    llarp::SecretKey m_idKey;
    llarp::SecretKey m_encKey;
    llarp::SecretKey m_transportKey;

    /// Backup each key file (by copying, e.g. foo -> foo.bak)
    bool
    backupKeyFilesByMoving() const;

    /// Load the key at a given filepath or create it
    ///
    /// @param keygen is a function that will generate the key if needed
    static bool
    loadOrCreateKey(
        const std::string& filepath,
        llarp::SecretKey& key,
        std::function<void(llarp::SecretKey& key)> keygen);
  };

}  // namespace llarp

#endif
