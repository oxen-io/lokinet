#include "auth.hpp"
#include <unordered_map>

namespace llarp::service
{
  /// maybe get auth result from string
  std::optional<AuthResultCode>
  ParseAuthResultCode(std::string data)
  {
    std::unordered_map<std::string, AuthResultCode> values = {
        {"OKAY", AuthResultCode::eAuthAccepted},
        {"REJECT", AuthResultCode::eAuthRejected},
        {"PAYME", AuthResultCode::eAuthPaymentRequired},
        {"LIMITED", AuthResultCode::eAuthRateLimit}};
    auto itr = values.find(data);
    if (itr == values.end())
      return std::nullopt;
    return itr->second;
  }

  AuthType
  ParseAuthType(std::string data)
  {
    std::unordered_map<std::string, AuthType> values = {{"lmq", AuthType::eAuthTypeLMQ},
                                                        {"whitelist", AuthType::eAuthTypeWhitelist},
                                                        {"none", AuthType::eAuthTypeNone}};
    const auto itr = values.find(data);
    if (itr == values.end())
      throw std::invalid_argument("no such auth type: " + data);
    return itr->second;
  }

  /// turn an auth result code into an int
  uint64_t
  AuthResultCodeAsInt(AuthResultCode code)
  {
    switch (code)
    {
      case AuthResultCode::eAuthAccepted:
        return 0;
      case AuthResultCode::eAuthRejected:
        return 1;
      case AuthResultCode::eAuthFailed:
        return 2;
      case AuthResultCode::eAuthRateLimit:
        return 3;
      case AuthResultCode::eAuthPaymentRequired:
        return 4;
      default:
        return -1;
    }
  }

  /// may turn an int into an auth result code
  std::optional<AuthResultCode>
  AuthResultCodeFromInt(uint64_t code)
  {
    switch (code)
    {
      case 0:
        return AuthResultCode::eAuthAccepted;
      case 1:
        return AuthResultCode::eAuthRejected;
      case 2:
        return AuthResultCode::eAuthFailed;
      case 3:
        return AuthResultCode::eAuthRateLimit;
      case 4:
        return AuthResultCode::eAuthPaymentRequired;
      default:
        return std::nullopt;
    }
  }

}  // namespace llarp::service
