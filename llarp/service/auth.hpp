#pragma once
#include <optional>
#include <string>
#include <funcional>
#include "address.hpp"
#include "handler.hpp"

namespace llarp::service
{
  /// authentication status
  enum class AuthResult
  {
    /// explicitly accepted
    eAuthAccepted,
    /// explicitly rejected
    eAuthRejected,
    /// attempt failed
    eAuthFailed,
    /// attempt rate limited
    eAuthRateLimit,
    /// need mo munny
    eAuthPaymentRequired
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
        service::Address from, service::ConvoTag tag, std::function<void(AuthResult)> hook) = 0;
  };
}  // namespace llarp::service
