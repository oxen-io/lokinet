#pragma once
#include <optional>
#include <string>
#include <functional>
#include "address.hpp"
#include "handler.hpp"
#include <crypto/types.hpp>

namespace llarp::service
{
  /// authentication status
  enum AuthResult
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

  /// maybe get auth result from string
  std::optional<AuthResult>
  ParseAuthResult(std::string data);

  struct IAuthPolicy
  {
    ~IAuthPolicy() = default;

    /// asynchronously determine if we accept new convotag from remote service, call hook with
    /// result later
    virtual void
    AuthenticateAsync(
        std::shared_ptr<llarp::service::ProtocolMessage> msg,
        std::function<void(AuthResult)> hook) = 0;
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
