#ifndef LLARP_KEY_MANAGER_HPP
#define LLARP_KEY_MANAGER_HPP

#include <atomic>
#include <config/config.hpp>
#include <crypto/types.hpp>
#include <router_contact.hpp>

namespace llarp
{
  /// KeyManager manages the cryptographic keys stored on disk for the local
  /// node. This includes private keys as well as the self-signed router contact
  /// file (e.g. "self.signed").
  ///
  /// Keys are either read from disk if they exist and are valid (see below) or
  /// are generated and written to disk.
  ///
  /// In addition, the KeyManager detects when the keys obsolete (e.g. as a
  /// result of a software upgrade) and backs up existing keys before writing
  /// out new ones.

  struct KeyManager
  {
    /// Utility function to backup a file by moving it. Attempts to find a new
    /// filename based on the original that doesn't exist, then moves it. The
    /// pattern used is originalFile.N.bak where N is the lowest integer
    /// matching a filename that doesn't exist.
    ///
    /// @param filepath is the name of the original file to backup.
    /// @return true if the file could be moved or didn't exist, false otherwise
    static bool
    backupFileByMoving(const fs::path& filepath);

    /// Constructor
    KeyManager();

    /// Initializes keys using the provided config, loading from disk and/or
    /// lokid via HTTP request.
    ///
    /// NOTE: Must be called prior to obtaining any keys.
    /// NOTE: blocks on I/O
    ///
    /// @param config should be a prepared config object
    /// @param genIfAbsent determines whether or not we will create files if
    /// they
    ///        do not exist.
    /// @return true on success, false otherwise
    bool
    initialize(const llarp::Config& config, bool genIfAbsent);

    /// Obtain the self-signed RouterContact
    ///
    /// @param rc (out) will be modified to contian the RouterContact
    /// @return true on success, false otherwise
    bool
    getRouterContact(llarp::RouterContact& rc) const;

    /// Return whether or not we need to backup keys as we load them
    bool
    needBackup() const
    {
      return m_needBackup;
    }

    llarp::SecretKey identityKey;
    llarp::SecretKey encryptionKey;
    llarp::SecretKey transportKey;

   private:
    fs::path m_rcPath;
    fs::path m_idKeyPath;
    fs::path m_encKeyPath;
    fs::path m_transportKeyPath;
    std::atomic_bool m_initialized;
    std::atomic_bool m_needBackup;

    bool m_usingLokid = false;
    std::string m_lokidRPCAddr = "127.0.0.1:22023";
    std::string m_lokidRPCUser;
    std::string m_lokidRPCPassword;

    /// Backup each key file (by copying, e.g. foo -> foo.bak)
    bool
    backupKeyFilesByMoving() const;

    /// Load the key at a given filepath or create it
    ///
    /// @param keygen is a function that will generate the key if needed
    static bool
    loadOrCreateKey(const fs::path& filepath, llarp::SecretKey& key,
                    std::function< void(llarp::SecretKey& key) > keygen);

    /// Requests the identity key from lokid via HTTP (curl)
    bool
    loadIdentityFromLokid();
  };

}  // namespace llarp

#endif
