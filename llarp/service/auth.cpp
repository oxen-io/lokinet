#include "auth.hpp"

#include "protocol.hpp"

#include <llarp/router/router.hpp>
#include <llarp/util/fs.hpp>
#include <llarp/util/str.hpp>

#include <unordered_map>

namespace llarp::service
{
    /// maybe get auth result from string
    std::optional<AuthCode> parse_auth_code(std::string data)
    {
        std::unordered_map<std::string, AuthCode> values = {
            {"OKAY", AuthCode::ACCEPTED},
            {"REJECT", AuthCode::REJECTED},
            {"PAYME", AuthCode::PAYMENT_REQUIRED},
            {"LIMITED", AuthCode::RATE_LIMIT}};
        auto itr = values.find(data);
        if (itr == values.end())
            return std::nullopt;
        return itr->second;
    }

    AuthType parse_auth_type(std::string data)
    {
        std::unordered_map<std::string, AuthType> values = {
            {"file", AuthType::FILE},
            {"lmq", AuthType::OMQ},
            {"whitelist", AuthType::WHITELIST},
            {"none", AuthType::NONE}};
        const auto itr = values.find(data);
        if (itr == values.end())
            throw std::invalid_argument("no such auth type: " + data);
        return itr->second;
    }

    AuthFileType parse_auth_file_type(std::string data)
    {
        std::unordered_map<std::string, AuthFileType> values = {
            {"plain", AuthFileType::PLAIN},
            {"plaintext", AuthFileType::PLAIN},
            {"hashed", AuthFileType::HASHES},
            {"hashes", AuthFileType::HASHES},
            {"hash", AuthFileType::HASHES}};
        const auto itr = values.find(data);
        if (itr == values.end())
            throw std::invalid_argument("no such auth file type: " + data);
#ifndef HAVE_CRYPT
        if (itr->second == AuthFileType::HASHES)
            throw std::invalid_argument("unsupported auth file type: " + data);
#endif
        return itr->second;
    }

    /// turn an auth result code into an int
    uint64_t auth_code_to_int(AuthCode code)
    {
        return static_cast<std::underlying_type_t<AuthCode>>(code);
    }
    /// may turn an int into an auth result code
    std::optional<AuthCode> int_to_auth_code(uint64_t code)
    {
        switch (code)
        {
            case 0:
                return AuthCode::ACCEPTED;
            case 1:
                return AuthCode::REJECTED;
            case 2:
                return AuthCode::FAILED;
            case 3:
                return AuthCode::RATE_LIMIT;
            case 4:
                return AuthCode::PAYMENT_REQUIRED;
            default:
                return std::nullopt;
        }
    }

    class FileAuthPolicy : public IAuthPolicy, public std::enable_shared_from_this<FileAuthPolicy>
    {
        const std::set<fs::path> m_Files;
        const AuthFileType m_Type;
        Router* const m_Router;
        mutable util::Mutex m_Access;
        std::unordered_set<ConvoTag> m_Pending;
        /// returns an auth result for a auth info challange, opens every file until it finds a
        /// token matching it this is expected to be done in the IO thread
        AuthResult check_files(const AuthInfo& info) const
        {
            for (const auto& f : m_Files)
            {
                fs::ifstream i{f};
                std::string line{};
                while (std::getline(i, line))
                {
                    // split off comments
                    const auto parts = split_any(line, "#;", true);
                    if (auto part = parts[0]; not parts.empty() and not parts[0].empty())
                    {
                        // split off whitespaces and check password
                        if (check_passwd(std::string{TrimWhitespace(part)}, info.token))
                            return AuthResult{AuthCode::ACCEPTED, "accepted by whitelist"};
                    }
                }
            }
            return AuthResult{AuthCode::REJECTED, "rejected by whitelist"};
        }

        bool check_passwd(std::string hash, std::string challenge) const
        {
            switch (m_Type)
            {
                case AuthFileType::PLAIN:
                    return hash == challenge;
                case AuthFileType::HASHES:
#ifdef HAVE_CRYPT
                    return crypto::check_passwd_hash(std::move(hash), std::move(challenge));
#endif
                default:
                    return false;
            }
        }

       public:
        FileAuthPolicy(Router* r, std::set<fs::path> files, AuthFileType filetype)
            : m_Files{std::move(files)}, m_Type{filetype}, m_Router{r}
        {}

        void authenticate_async(
            std::shared_ptr<ProtocolMessage> msg, std::function<void(std::string, bool)> hook) override
        {
            auto reply =
                m_Router->loop()->make_caller([tag = msg->tag, hook, self = shared_from_this()](AuthResult result) {
                    {
                        util::Lock _lock{self->m_Access};
                        self->m_Pending.erase(tag);
                    }
                    hook(result.reason, result.code == AuthCode::ACCEPTED);
                });
            {
                util::Lock _lock{m_Access};
                m_Pending.emplace(msg->tag);
            }
            if (msg->proto == ProtocolType::Auth)
            {
                m_Router->queue_disk_io([self = shared_from_this(),
                                         auth = AuthInfo{std::string{
                                             reinterpret_cast<const char*>(msg->payload.data()), msg->payload.size()}},
                                         reply]() {
                    try
                    {
                        reply(self->check_files(auth));
                    }
                    catch (std::exception& ex)
                    {
                        reply(AuthResult{AuthCode::FAILED, ex.what()});
                    }
                });
            }
            else
                reply(AuthResult{AuthCode::REJECTED, "protocol error"});
        }
        bool auth_async_pending(ConvoTag tag) const override
        {
            util::Lock _lock{m_Access};
            return m_Pending.count(tag);
        }
    };

    std::shared_ptr<IAuthPolicy> make_file_auth_policy(Router* r, std::set<fs::path> files, AuthFileType filetype)
    {
        return std::make_shared<FileAuthPolicy>(r, std::move(files), filetype);
    }

}  // namespace llarp::service
