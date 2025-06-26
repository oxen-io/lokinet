#include "auth.hpp"

namespace llarp
{
    /// maybe get auth result from string
    std::optional<auth::AuthCode> parse_auth_code(std::string data)
    {
        std::unordered_map<std::string, auth::AuthCode> values = {
            {"OKAY", auth::AuthCode::ACCEPTED},
            {"REJECT", auth::AuthCode::REJECTED},
            {"PAYME", auth::AuthCode::PAYMENT_REQUIRED},
            {"LIMITED", auth::AuthCode::RATE_LIMIT}};
        auto itr = values.find(data);
        if (itr == values.end())
            return std::nullopt;
        return itr->second;
    }

    /// get an auth type from a string
    /// throws std::invalid_argument if arg is invalid
    auth::AuthType parse_auth_type(std::string data)
    {
        std::unordered_map<std::string, auth::AuthType> values = {
            {"file", auth::AuthType::FILE},
            {"lmq", auth::AuthType::OMQ},
            {"whitelist", auth::AuthType::WHITELIST},
            {"none", auth::AuthType::NONE}};
        const auto itr = values.find(data);
        if (itr == values.end())
            throw std::invalid_argument("no such auth type: " + data);
        return itr->second;
    }

    /// get an auth file type from a string
    /// throws std::invalid_argument if arg is invalid
    auth::AuthFileType parse_auth_file_type(std::string data)
    {
        std::unordered_map<std::string, auth::AuthFileType> values = {
            {"plain", auth::AuthFileType::PLAIN},
            {"plaintext", auth::AuthFileType::PLAIN},
            {"hashed", auth::AuthFileType::HASHES},
            {"hashes", auth::AuthFileType::HASHES},
            {"hash", auth::AuthFileType::HASHES}};
        const auto itr = values.find(data);
        if (itr == values.end())
            throw std::invalid_argument("no such auth file type: " + data);
#ifndef HAVE_CRYPT
        if (itr->second == auth::AuthFileType::HASHES)
            throw std::invalid_argument("unsupported auth file type: " + data);
#endif
        return itr->second;
    }
}  // namespace llarp
