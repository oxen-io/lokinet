#pragma once

#include "auth.hpp"

namespace llarp
{
    class Router;
}
namespace llarp::auth
{
    struct SessionAuthPolicy final : public AuthPolicy
    {
      private:
        const bool _is_snode_service{false};
        const bool _is_exit_service{false};

        Ed25519SecretKey _session_key;
        NetworkAddress _remote;

      public:
        SessionAuthPolicy(Router& r, RouterID& remote, bool is_snode, bool is_exit = false);

        bool load_identity_from_file(const char* fname);

        std::optional<std::string_view> fetch_auth_token();

        const Ed25519SecretKey& session_key() const { return _session_key; }

        bool is_snode_service() const { return _is_snode_service; }

        bool is_exit_service() const { return _is_exit_service; }
    };

}  // namespace llarp::auth
