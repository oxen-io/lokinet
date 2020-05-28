#include "auth.hpp"
#include <unordered_map>

namespace llarp::service
{
  /// maybe get auth result from string
  std::optional<AuthResult>
  ParseAuthResult(std::string data)
  {
    static thread_local std::unordered_map<std::string, AuthResult> values = {
        {"OKAY", AuthResult::eAuthAccepted},
        {"REJECT", AuthResult::eAuthRejected},
        {"PAYME", AuthResult::eAuthPaymentRequired},
        {"LIMITED", AuthResult::eAuthRateLimit}};
    auto itr = values.find(data);
    if (itr == values.end())
      return std::nullopt;
    return itr->second;
  }
}  // namespace llarp::service
