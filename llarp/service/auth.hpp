#pragma once
#include <optional>
#include <string>
#include <functional>
#include "address.hpp"
#include "handler.hpp"
#include <llarp/crypto/types.hpp>

namespace llarp::service
{
  /// authentication status code
  enum class AuthResultCode : uint64_t
  {
    /// explicitly accepted
    eAuthAccepted = 0,
    /// explicitly rejected
    eAuthRejected = 1,
    /// attempt failed
    eAuthFailed = 2,
    /// attempt rate limited
    eAuthRateLimit = 3,
    /// need mo munny
    eAuthPaymentRequired = 4
  };

  /// turn an auth result code into an int
  uint64_t
  AuthResultCodeAsInt(AuthResultCode code);

  /// may turn an int into an auth result code
  std::optional<AuthResultCode>
  AuthResultCodeFromInt(uint64_t code);

  /// auth result object with code and reason
  struct AuthResult
  {
    AuthResultCode code;
    std::string reason;
  };

  /// maybe get auth result from string
  std::optional<AuthResultCode>
  ParseAuthResultCode(std::string data);

  struct IAuthPolicy
  {
    ~IAuthPolicy() = default;

    /// asynchronously determine if we accept new convotag from remote service, call hook with
    /// result later
    virtual void
    AuthenticateAsync(
        std::shared_ptr<ProtocolMessage> msg, std::function<void(AuthResult)> hook) = 0;

    /// return true if we are asynchronously processing authentication on this convotag
    virtual bool
    AsyncAuthPending(ConvoTag tag) const = 0;
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
    eAuthTypeNone,
    /// manual whitelist
    eAuthTypeWhitelist,
    /// LMQ server
    eAuthTypeLMQ
  };

  /// get an auth type from a string
  /// throws std::invalid_argument if arg is invalid
  AuthType
  ParseAuthType(std::string arg);

}  // namespace llarp::service
