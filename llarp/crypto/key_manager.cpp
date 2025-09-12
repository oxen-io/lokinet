#include "key_manager.hpp"

#include "crypto.hpp"
#include "keys.hpp"

#include <llarp/config/config.hpp>
#include <llarp/util/file.hpp>
#include <llarp/util/logging.hpp>

namespace llarp
{
    static auto logcat = log::Cat("keymanager");

    void KeyManager::load_from_file(Ed25519SecretKey& key, const std::filesystem::path& fname)
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);

        // TODO FIXME: do we want to allow hex or other non-binary encodings here?
        auto tmp = util::file_to_string(fname, 64);
        if (tmp.size() != 64)
            throw std::invalid_argument{"Invalid key file {}: Expected 64 bytes, not {}"_format(fname, tmp.size())};

        std::memcpy(key.data(), tmp.data(), 64);
        if (!key.check_pubkey())
            throw std::invalid_argument{"Invalid key file {}: Keypair seed and pubkey do not match"};
    }

    bool KeyManager::write_to_file(const Ed25519SecretKey& key, const std::filesystem::path& fname)
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);
        try
        {
            util::buffer_to_file(fname, key.to_view());
        }
        catch (const std::exception& e)
        {
            log::error(logcat, "Failed to write keypair to file: {}", e.what());
            return false;
        }

        return true;
    }

    KeyManager::KeyManager(const Config& config, bool is_relay)
    {
        if (not is_relay)
        {
            if (config.network.keyfile)
            {
                load_from_file(secret_key, *config.network.keyfile);
                log::info(logcat, "Successfully loaded persistent client key from config path");
            }
            else
            {
                log::debug(logcat, "Client generating secret key...");
                secret_key = crypto::generate_ed25519();
            }

            public_key.assign(secret_key.pubkey_span());

            log::info(logcat, "Client public key: {}", public_key);
        }
        // else nothing to do: router's identity self.signed is always regenerated on the fly from
        // the keys we get from oxend.
    }

    void KeyManager::update_idkey(Ed25519SecretKey&& newkey)
    {
        secret_key = std::move(newkey);
        public_key.assign(secret_key.pubkey_span());
        log::info(logcat, "Relay key manager updated secret key; new public key: {}", public_key);
    }

}  // namespace llarp
