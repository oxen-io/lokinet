#ifndef LLARP_KEY_MANAGER_HPP
#define LLARP_KEY_MANAGER_HPP

#include <atomic>
#include <config/config.hpp>
#include <crypto/types.hpp>
#include <router_contact.hpp>
#include <unordered_map>

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

    using KeyGenerator = std::function<void(llarp::SecretKey& key)>;
    using KeyWriter = std::function<bool(const llarp::SecretKey& key, const std::string filepath)>;

    /// Constructor
    KeyManager();

    /// Initializes keys using the provided config, loading from disk and/or lokid
    /// via HTTP request.
    ///
    /// NOTE: Must be called prior to obtaining any keys.
    /// NOTE: blocks on I/O
    ///
    /// @param config should be a prepared config object
    /// @param genIfAbsent determines whether or not we will create files if they
    ///        do not exist.
    /// @return true on success, false otherwise
    bool
    initialize(const llarp::Config& config, bool genIfAbsent);

    /// Obtain the identity key (e.g. ~/.lokinet/identity.private)
    ///
    /// @return a reference to the identity key
    const llarp::SecretKey&
    getIdentityKey() const;

    /// Set the identity key. This does not write anything to disk.
    ///
    /// @param key is the key that will be copied-from.
    void
    setIdentityKey(const llarp::SecretKey& key);

    /// Obtain the encryption key (e.g. ~/.lokinet/encryption.private)
    ///
    /// @return a reference to the encryption key
    const llarp::SecretKey&
    getEncryptionKey() const;

    /// Set the encryption key. This does not write anything to disk.
    ///
    /// @param key is the key that will be copied-from.
    void
    setEncryptionKey(const llarp::SecretKey& key);

    /// Obtain the transport key (e.g. ~/.lokinet/transport.private)
    ///
    /// @return a reference to the transport key
    const llarp::SecretKey&
    getTransportKey() const;

    /// Set the transport key. This does not write anything to disk.
    ///
    /// @param key is the key that will be copied-from.
    void
    setTransportKey(const llarp::SecretKey& key);

    /// Obtain the self-signed RouterContact
    ///
    /// @param rc (out) will be modified to contian the RouterContact
    /// @return true on success, false otherwise
    bool
    getRouterContact(llarp::RouterContact& rc) const;

    /// Load a key at a given filepath and associate it with the given id.
    ///
    /// If the KeyManager determined during initialize() that keys need to be
    /// regenerated, and genIfAbsent is true, the keys will be backed up and
    /// new keys will be generated.
    ///
    /// @param id is the unique id to assicate the loaded key with
    /// @param filepath is the file path to load the key from
    /// @param genIfAbsent determines whether we will create the key if it exists
    /// @param keygen should be a function that will generate the secret key
    /// @param writer should be a function that will write the key to file (if needed)
    /// @return true on success, false otherwise
    bool
    loadOrCreateOtherKey(std::string id, const std::string& filepath,
                        bool genIfAbsent, KeyGenerator keygen, KeyWriter writer);

    /// Obtain a key by its id
    ///
    /// @return a key (by value) for the given id, or a zero-key if none exists
    llarp::SecretKey
    getOtherKey(const std::string& id) const;

  private:

    std::string m_rcPath;
    std::string m_idKeyPath;
    std::string m_encKeyPath;
    std::string m_transportKeyPath;
    std::atomic_bool m_initialized;
    std::atomic_bool m_backupRequired;

    bool m_usingLokid = false;
    std::string m_lokidRPCAddr = "127.0.0.1:22023";
    std::string m_lokidRPCUser;
    std::string m_lokidRPCPassword;

    llarp::SecretKey m_idKey;
    llarp::SecretKey m_encKey;
    llarp::SecretKey m_transportKey;
    std::unordered_map<std::string, llarp::SecretKey> m_otherKeys;

    /// Backup each key file (by copying, e.g. foo -> foo.bak)
    ///
    /// NOTE: this does not back up other (e.g. snapp) keys
    bool
    backupMainKeys() const;

    /// Load the key at a given filepath or create it
    ///
    /// @param keygen is a function that will generate the key if needed
    static bool
    loadOrCreateKey(
        const std::string& filepath,
        llarp::SecretKey& key,
        KeyGenerator keygen,
        KeyWriter writer);

    /// Requests the identity key from lokid via HTTP (curl)
    bool
    loadIdentityFromLokid();
  };

}  // namespace llarp

#endif
