#pragma once
#include <llarp/crypto/types.hpp>
#include <optional>
#include <string>
#include <functional>

#include "address.hpp"

namespace llarp
{
  struct Router;
}

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

  struct ProtocolMessage;
  struct ConvoTag;

  /// maybe get auth result from string
  std::optional<AuthResultCode>
  ParseAuthResultCode(std::string data);

  struct IAuthPolicy
  {
    virtual ~IAuthPolicy() = default;

    /// asynchronously determine if we accept new convotag from remote service, call hook with
    /// result later
    virtual void
    AuthenticateAsync(
        std::shared_ptr<ProtocolMessage> msg, std::function<void(std::string, bool)> hook) = 0;

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
    eAuthTypeLMQ,
    /// static file
    eAuthTypeFile,
  };

  /// how to interpret an file for auth
  enum class AuthFileType
  {
    eAuthFilePlain,
    eAuthFileHashes,
  };

  /// get an auth type from a string
  /// throws std::invalid_argument if arg is invalid
  AuthType
  ParseAuthType(std::string arg);

  /// get an auth file type from a string
  /// throws std::invalid_argument if arg is invalid
  AuthFileType
  ParseAuthFileType(std::string arg);

  /// make an IAuthPolicy that reads out of a static file
  std::shared_ptr<IAuthPolicy>
  MakeFileAuthPolicy(Router*, std::set<fs::path> files, AuthFileType fileType);

}  // namespace llarp::service
