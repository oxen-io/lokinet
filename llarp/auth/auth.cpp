#include "auth.hpp"

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

    /// get an auth type from a string
    /// throws std::invalid_argument if arg is invalid
    AuthType parse_type(std::string_view data)
    {
        if (auto it = types.find(data); it != types.end())
            return it->second;
        throw std::invalid_argument("no such auth type: {}"_format(data));
    }

    static const std::unordered_map<std::string_view, AuthFileType> file_types = {
        {"plain"sv, AuthFileType::PLAIN},
        {"plaintext"sv, AuthFileType::PLAIN},
        {"hashed"sv, AuthFileType::HASHES},
        {"hashes"sv, AuthFileType::HASHES},
        {"hash"sv, AuthFileType::HASHES}};

    /// get an auth file type from a string
    /// throws std::invalid_argument if arg is invalid
    AuthFileType parse_file_type(std::string_view data)
    {
        const auto itr = file_types.find(data);
        if (itr == file_types.end())
            throw std::invalid_argument{"no such auth file type: {}"_format(data)};
#ifndef HAVE_CRYPT
        if (itr->second == AuthFileType::HASHES)
            throw std::invalid_argument{"unsupported auth file type: {}"_format(data)};
#endif
        return itr->second;
    }
}  // namespace llarp::auth
