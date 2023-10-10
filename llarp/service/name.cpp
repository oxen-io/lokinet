#include "name.hpp"
#include <llarp/crypto/crypto.hpp>
#include <llarp/util/str.hpp>

namespace llarp::service
{
  std::optional<Address>
  EncryptedName::Decrypt(std::string_view name) const
  {
    if (ciphertext.empty())
      return std::nullopt;
    const auto crypto = CryptoManager::instance();
    const auto maybe = crypto->maybe_decrypt_name(ciphertext, nonce, name);
    if (maybe.has_value())
      return Address{*maybe};
    return std::nullopt;
  }

  bool
  is_valid_name(std::string_view lnsName)
  {
    // make sure it ends with .loki because no fucking shit right?
    if (not ends_with(lnsName, ".loki"))
      return false;
    // strip off .loki suffix
    lnsName = lnsName.substr(0, lnsName.find_last_of('.'));

    // ensure chars are sane
    for (const auto ch : lnsName)
    {
      if (ch == '-')
        continue;
      if (ch == '.')
        continue;
      if (ch >= 'a' and ch <= 'z')
        continue;
      if (ch >= '0' and ch <= '9')
        continue;
      return false;
    }
    // split into domain parts
    const auto parts = split(lnsName, ".");
    // get root domain
    const auto primaryName = parts[parts.size() - 1];
    constexpr size_t MaxNameLen = 32;
    constexpr size_t MaxPunycodeNameLen = 63;
    // check against lns name blacklist
    if (primaryName == "localhost")
      return false;
    if (primaryName == "loki")
      return false;
    if (primaryName == "snode")
      return false;
    // check for dashes
    if (primaryName.find("-") == std::string_view::npos)
      return primaryName.size() <= MaxNameLen;
    // check for dashes and end or beginning
    if (*primaryName.begin() == '-' or *(primaryName.end() - 1) == '-')
      return false;
    // check for punycode name length
    if (primaryName.size() > MaxPunycodeNameLen)
      return false;
    // check for xn--
    return (primaryName[2] == '-' and primaryName[3] == '-')
        ? (primaryName[0] == 'x' and primaryName[1] == 'n')
        : true;
  }

}  // namespace llarp::service
