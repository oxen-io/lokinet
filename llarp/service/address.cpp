#include <service/address.hpp>
#include <crypto/crypto.hpp>
#include <algorithm>

namespace llarp
{
  namespace service
  {
    const std::set<std::string> Address::AllowedTLDs = {".loki", ".snode"};

    bool
    Address::PermitTLD(const char* tld)
    {
      std::string gtld(tld);
      std::transform(gtld.begin(), gtld.end(), gtld.begin(), ::tolower);
      return AllowedTLDs.count(gtld) != 0;
    }

    std::string
    Address::ToString(const char* tld) const
    {
      if (!PermitTLD(tld))
        return "";
      char tmp[(1 + 32) * 2] = {0};
      std::string str = Base32Encode(*this, tmp);
      if (subdomain.size())
        str = subdomain + "." + str;
      return str + tld;
    }

    bool
    Address::FromString(const std::string& str, const char* tld)
    {
      if (!PermitTLD(tld))
        return false;
      static auto lowercase = [](const std::string s, bool stripDots) -> std::string {
        std::string ret(s.size(), ' ');
        std::transform(s.begin(), s.end(), ret.begin(), [stripDots](const char& ch) -> char {
          if (ch == '.' && stripDots)
            return ' ';
          return ::tolower(ch);
        });
        return ret.substr(0, ret.find_last_of(' '));
      };
      const auto pos = str.find_last_of('.');
      if (pos == std::string::npos)
        return false;
      if (str.substr(pos) != tld)
        return false;
      auto sub = str.substr(0, pos);
      // set subdomains if they are there
      const auto idx = sub.find_last_of('.');
      if (idx != std::string::npos)
      {
        subdomain = lowercase(sub.substr(0, idx), false);
        sub = sub.substr(idx + 1);
      }
      // make sure it's lowercase
      return Base32Decode(lowercase(sub, true), *this);
    }

    dht::Key_t
    Address::ToKey() const
    {
      PubKey k;
      CryptoManager::instance()->derive_subkey(k, PubKey(data()), 1);
      return dht::Key_t{k.as_array()};
    }

  }  // namespace service
}  // namespace llarp
