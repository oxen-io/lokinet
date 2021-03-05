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
    std::unordered_map<std::string, AuthType> values = {
        {"lmq", AuthType::eAuthTypeLMQ},
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
    return static_cast<std::underlying_type_t<AuthResultCode>>(code);
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
