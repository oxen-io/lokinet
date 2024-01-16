#pragma once
#include "address.hpp"

#include <llarp/crypto/types.hpp>

#include <functional>
#include <optional>
#include <string>

namespace llarp
{
  struct Router;
}

namespace llarp::service
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

  /// turn an auth result code into an int
  uint64_t
  auth_code_to_int(AuthCode code);

  /// may turn an int into an auth result code
  std::optional<AuthCode>
  int_to_auth_code(uint64_t code);

  /// auth result object with code and reason
  struct AuthResult
  {
    AuthCode code;
    std::string reason;
  };

  struct ProtocolMessage;
  struct ConvoTag;

  /// maybe get auth result from string
  std::optional<AuthCode>
  parse_auth_code(std::string data);

  struct IAuthPolicy
  {
    virtual ~IAuthPolicy() = default;

    /// asynchronously determine if we accept new convotag from remote service, call hook with
    /// result later
    virtual void
    authenticate_async(
        std::shared_ptr<ProtocolMessage> msg, std::function<void(std::string, bool)> hook) = 0;

    /// return true if we are asynchronously processing authentication on this convotag
    virtual bool
    auth_async_pending(ConvoTag tag) const = 0;
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
    /// LMQ server
    OMQ,
    /// static file
    FILE,
  };

  /// how to interpret an file for auth
  enum class AuthFileType
  {
    PLAIN,
    HASHES,
  };

  /// get an auth type from a string
  /// throws std::invalid_argument if arg is invalid
  AuthType
  parse_auth_type(std::string arg);

  /// get an auth file type from a string
  /// throws std::invalid_argument if arg is invalid
  AuthFileType
  parse_auth_file_type(std::string arg);

  /// make an IAuthPolicy that reads out of a static file
  std::shared_ptr<IAuthPolicy>
  make_file_auth_policy(Router*, std::set<fs::path> files, AuthFileType fileType);

}  // namespace llarp::service
