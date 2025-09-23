#pragma once

#include <llarp/constants/files.hpp>
#include <llarp/contact/router_id.hpp>

namespace llarp
{
    struct Config;

    // KeyManager manages the cryptographic keys stored on disk for the local
    // node. This includes private keys as well as the self-signed router contact
    // file (e.g. "self.signed").
    //
    // Keys are either read from disk if they exist and are valid (see below) or
    // are generated and written to disk.
    struct KeyManager
    {
        friend class Router;

      private:
        KeyManager() = default;
        KeyManager(const Config& config, bool is_relay);

        Ed25519SecretKey secret_key;
        RouterID public_key;

        void update_idkey(Ed25519SecretKey&& newkey);

      public:
        const RouterID& router_id() const { return public_key; }

        // Helper functions to load a key; these are used by KeyManager itself, but are exposed as
        // they also have some uses for key loading outside KeyManager.
        static void load_from_file(Ed25519SecretKey& key, const std::filesystem::path& fname);
        static bool write_to_file(const Ed25519SecretKey& key, const std::filesystem::path& fname);
    };

}  // namespace llarp
