#include "auth.hpp"

#include <oxenmq/oxenmq.h>

namespace llarp::auth
{
    static const std::unordered_map<std::string_view, AuthCode> codes = {
        {"OKAY"sv, AuthCode::ACCEPTED},
        {"REJECT"sv, AuthCode::REJECTED},
        {"PAYME"sv, AuthCode::PAYMENT_REQUIRED},
        {"LIMITED"sv, AuthCode::RATE_LIMIT}};

    /// maybe get auth result from string
    std::optional<AuthCode> parse_code(std::string_view data)
    {
        if (auto it = codes.find(data); it != codes.end())
            return it->second;
        return std::nullopt;
    }

    static const std::unordered_map<std::string_view, AuthType> types = {
        {"file"sv, AuthType::FILE},
        {"lmq"sv, AuthType::OMQ},
        {"whitelist"sv, AuthType::WHITELIST},
        {"none"sv, AuthType::NONE}};

}  // namespace llarp::auth
