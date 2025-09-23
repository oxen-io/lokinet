#pragma once

#include <llarp/address/address.hpp>
#include <llarp/contact/router_id.hpp>
#include <llarp/crypto/types.hpp>
#include <llarp/util/str.hpp>
#include <llarp/util/thread/threading.hpp>

#include <functional>
#include <optional>
#include <string>
#include <unordered_set>

namespace llarp
{
    class Router;
}
namespace llarp::auth
{
    /// authentication status code
    enum class AuthCode : uint64_t
    {
        /// explicitly accepted
        ACCEPTED = 0,
        /// explicitly rejected
        REJECTED = 1,
        /// attempt failed
        FAILED = 2,
        /// attempt rate limited
        RATE_LIMIT = 3,
        /// need mo munny
        PAYMENT_REQUIRED = 4
    };

    /// auth result object with code and reason
    struct AuthResult
    {
        AuthCode code;
        std::string reason;
    };

    /// info needed by clients in order to authenticate to a remote endpoint
    struct AuthInfo
    {
        std::string token;
    };

    /// what kind of backend to use for auth
    enum class AuthType
    {
        /// no authentication
        NONE,
        /// manual whitelist
        WHITELIST,
        /// OMQ server
        OMQ,
        /// static file
        FILE,
    };

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

    /// maybe get auth result from string
    std::optional<AuthCode> parse_code(std::string_view data);

}  // namespace llarp::auth
