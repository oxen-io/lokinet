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
}  // namespace llarp::service
