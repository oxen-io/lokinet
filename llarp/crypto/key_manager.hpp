#pragma once

#include "types.hpp"

#include <llarp/contact/router_id.hpp>

#include <atomic>

namespace llarp
{
    struct Config;

    namespace handlers
    {
        class SessionEndpoint;
    }

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

        Ed25519SecretKey identity_key;
        Ed25519PrivateData identity_data;
        RouterID public_key;

        void update_idkey(Ed25519SecretKey&& newkey);

      public:
        const RouterID& router_id() const { return public_key; }

        Ed25519PrivateData derive_subkey(uint64_t domain = 1) const;
    };

}  // namespace llarp
