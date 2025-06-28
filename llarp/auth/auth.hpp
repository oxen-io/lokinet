#pragma once

#include "types.hpp"

#include <llarp/address/address.hpp>
#include <llarp/contact/router_id.hpp>
#include <llarp/contact/tag.hpp>
#include <llarp/crypto/types.hpp>
#include <llarp/util/str.hpp>
#include <llarp/util/thread/threading.hpp>

#include <oxenmq/oxenmq.h>

#include <functional>
#include <optional>
#include <string>
#include <unordered_set>

namespace llarp
{
    class Router;

    namespace auth
    {
        struct AuthPolicy
        {
          protected:
            Router& _router;

          public:
            AuthPolicy(Router& r) : _router{r} {}

            virtual ~AuthPolicy() = default;

            const Router& router() const { return _router; }

            Router& router() { return _router; }
        };

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

        struct FileAuthPolicy final : public AuthPolicy
        {
            FileAuthPolicy(Router& r, std::unordered_set<fs::path> files, AuthFileType filetype)
                : AuthPolicy{r}, _files{std::move(files)}, _type{filetype}
            {}

          private:
            const std::unordered_set<fs::path> _files;
            const AuthFileType _type;
            mutable util::Mutex _m;
            std::unordered_set<session_tag> _pending;
            /// returns an auth result for a auth info challange, opens every file until it finds a
            /// token matching it this is expected to be done in the IO thread
            AuthResult check_files(const AuthInfo& info) const;

            bool check_passwd(std::string hash, std::string challenge) const;
        };

        struct RPCAuthPolicy final : public AuthPolicy
        {
            explicit RPCAuthPolicy(Router& r, std::string url, std::string method, oxenmq::OxenMQ& omq);

            ~RPCAuthPolicy() override = default;

            void start();

          private:
            const std::string _endpoint;
            const std::string _method;
            // const std::unordered_set<NetworkAddress> _whitelist;
            // const std::unordered_set<std::string> _static_tokens;

            oxenmq::OxenMQ& _omq;
            std::optional<oxenmq::ConnectionID> _omq_conn;
            std::unordered_set<session_tag> _pending_sessions;
        };

        /// maybe get auth result from string
        std::optional<AuthCode> parse_code(std::string_view data);

        /// get an auth type from a string
        /// throws std::invalid_argument if arg is invalid
        AuthType parse_type(std::string_view data);

        /// get an auth file type from a string
        /// throws std::invalid_argument if arg is invalid
        AuthFileType parse_file_type(std::string_view data);

    }  // namespace auth

}  // namespace llarp
