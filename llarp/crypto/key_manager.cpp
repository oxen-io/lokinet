#include "key_manager.hpp"

#include "llarp/crypto/crypto.hpp"
#include "types.hpp"

#include <llarp/config/config.hpp>
#include <llarp/util/logging.hpp>

namespace llarp
{
    static auto logcat = log::Cat("keymanager");

    KeyManager::KeyManager(const Config& config, bool is_relay)
    {
        if (not is_relay)
        {
            if (config.network.keyfile.has_value() and identity_key.load_from_file(*config.network.keyfile))
            {
                log::info(logcat, "Successfully loaded persistent client key from config path");
            }
            else
            {
                log::debug(logcat, "Client generating identity key...");
                identity_key = crypto::generate_ed25519();
            }

            identity_data = identity_key.to_eddata();
            public_key.assign(identity_key.pubkey_span());

            log::info(logcat, "Client public key: {}", public_key);
        }
        // else nothing to do: router's identity self.signed is always regenerated on the fly from
        // the keys we get from oxend.
    }

    void KeyManager::update_idkey(Ed25519SecretKey&& newkey)
    {
        identity_key = std::move(newkey);
        identity_data = identity_key.to_eddata();
        public_key.assign(identity_key.pubkey_span());
        log::info(logcat, "Relay key manager updated secret key; new public key: {}", public_key);
    }

    Ed25519PrivateData KeyManager::derive_subkey(uint64_t domain) const
    {
        return identity_key.derive_private_subkey_data(domain);
    }

}  // namespace llarp
