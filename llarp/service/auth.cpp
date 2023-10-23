#include "auth.hpp"
#include "protocol.hpp"

#include <llarp/router/router.hpp>
#include <llarp/util/str.hpp>
#include <llarp/util/fs.hpp>

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
        {"file", AuthType::eAuthTypeFile},
        {"lmq", AuthType::eAuthTypeLMQ},
        {"whitelist", AuthType::eAuthTypeWhitelist},
        {"none", AuthType::eAuthTypeNone}};
    const auto itr = values.find(data);
    if (itr == values.end())
      throw std::invalid_argument("no such auth type: " + data);
    return itr->second;
  }

  AuthFileType
  ParseAuthFileType(std::string data)
  {
    std::unordered_map<std::string, AuthFileType> values = {
        {"plain", AuthFileType::eAuthFilePlain},
        {"plaintext", AuthFileType::eAuthFilePlain},
        {"hashed", AuthFileType::eAuthFileHashes},
        {"hashes", AuthFileType::eAuthFileHashes},
        {"hash", AuthFileType::eAuthFileHashes}};
    const auto itr = values.find(data);
    if (itr == values.end())
      throw std::invalid_argument("no such auth file type: " + data);
#ifndef HAVE_CRYPT
    if (itr->second == AuthFileType::eAuthFileHashes)
      throw std::invalid_argument("unsupported auth file type: " + data);
#endif
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

  class FileAuthPolicy : public IAuthPolicy, public std::enable_shared_from_this<FileAuthPolicy>
  {
    const std::set<fs::path> m_Files;
    const AuthFileType m_Type;
    Router* const m_Router;
    mutable util::Mutex m_Access;
    std::unordered_set<ConvoTag> m_Pending;
    /// returns an auth result for a auth info challange, opens every file until it finds a token
    /// matching it
    /// this is expected to be done in the IO thread
    AuthResult
    CheckFiles(const AuthInfo& info) const
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
            if (CheckPasswd(std::string{TrimWhitespace(part)}, info.token))
              return AuthResult{AuthResultCode::eAuthAccepted, "accepted by whitelist"};
          }
        }
      }
      return AuthResult{AuthResultCode::eAuthRejected, "rejected by whitelist"};
    }

    bool
    CheckPasswd(std::string hash, std::string challenge) const
    {
      switch (m_Type)
      {
        case AuthFileType::eAuthFilePlain:
          return hash == challenge;
        case AuthFileType::eAuthFileHashes:
          return crypto::check_passwd_hash(std::move(hash), std::move(challenge));
        default:
          return false;
      }
    }

   public:
    FileAuthPolicy(Router* r, std::set<fs::path> files, AuthFileType filetype)
        : m_Files{std::move(files)}, m_Type{filetype}, m_Router{r}
    {}

    void
    AuthenticateAsync(
        std::shared_ptr<ProtocolMessage> msg, std::function<void(std::string, bool)> hook) override
    {
      auto reply = m_Router->loop()->make_caller(
          [tag = msg->tag, hook, self = shared_from_this()](AuthResult result) {
            {
              util::Lock _lock{self->m_Access};
              self->m_Pending.erase(tag);
            }
            hook(result.reason, result.code == AuthResultCode::eAuthAccepted);
          });
      {
        util::Lock _lock{m_Access};
        m_Pending.emplace(msg->tag);
      }
      if (msg->proto == ProtocolType::Auth)
      {
        m_Router->queue_disk_io(
            [self = shared_from_this(),
             auth = AuthInfo{std::string{
                 reinterpret_cast<const char*>(msg->payload.data()), msg->payload.size()}},
             reply]() {
              try
              {
                reply(self->CheckFiles(auth));
              }
              catch (std::exception& ex)
              {
                reply(AuthResult{AuthResultCode::eAuthFailed, ex.what()});
              }
            });
      }
      else
        reply(AuthResult{AuthResultCode::eAuthRejected, "protocol error"});
    }
    bool
    AsyncAuthPending(ConvoTag tag) const override
    {
      util::Lock _lock{m_Access};
      return m_Pending.count(tag);
    }
  };

  std::shared_ptr<IAuthPolicy>
  MakeFileAuthPolicy(Router* r, std::set<fs::path> files, AuthFileType filetype)
  {
    return std::make_shared<FileAuthPolicy>(r, std::move(files), filetype);
  }

}  // namespace llarp::service
