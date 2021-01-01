#include "auth.hpp"
#include <unordered_map>

namespace llarp::service
{
  /// maybe get auth result from string
  std::optional<AuthResult>
  ParseAuthResult(std::string data)
  {
    static std::unordered_map<std::string, AuthResult> values = {
        {"OKAY", AuthResult::eAuthAccepted},
        {"REJECT", AuthResult::eAuthRejected},
        {"PAYME", AuthResult::eAuthPaymentRequired},
        {"LIMITED", AuthResult::eAuthRateLimit}};
    auto itr = values.find(data);
    if (itr == values.end())
      return std::nullopt;
    return itr->second;
  }

  std::string
  AuthResultDescription(AuthResult auth)
  {
    switch (auth)
    {
      case AuthResult::eAuthAccepted:
        return "accepted";
      case AuthResult::eAuthRejected:
        return "rejected";
      case AuthResult::eAuthPaymentRequired:
        return "payment required";
      case AuthResult::eAuthRateLimit:
        return "rate limit hit";
      default:
        return "unknown";
    }
  }

  AuthType
  ParseAuthType(std::string data)
  {
    static std::unordered_map<std::string, AuthType> values = {
        {"lmq", AuthType::eAuthTypeLMQ},
        {"whitelist", AuthType::eAuthTypeWhitelist},
        {"none", AuthType::eAuthTypeNone}};
    const auto itr = values.find(data);
    if (itr == values.end())
      throw std::invalid_argument("no such auth type: " + data);
    return itr->second;
  }
}  // namespace llarp::service
