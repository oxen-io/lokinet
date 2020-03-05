#ifndef LLARP_SERVICE_IDENTITY_HPP
#define LLARP_SERVICE_IDENTITY_HPP

#include <config/key_manager.hpp>
#include <constants/proto.hpp>
#include <crypto/types.hpp>
#include <memory>
#include <service/info.hpp>
#include <service/intro_set.hpp>
#include <service/vanity.hpp>
#include <util/buffer.hpp>

#include <tuple>

namespace llarp
{
  namespace service
  {
    // private keys
    struct Identity
    {
      SecretKey enckey;
      SecretKey signkey;
      PrivateKey derivedSignKey;
      PQKeyPair pq;
      uint64_t version = LLARP_PROTO_VERSION;
      VanityNonce vanity;

      // public service info
      ServiceInfo pub;

      // regenerate secret keys
      void
      RegenerateKeys();

      bool
      BEncode(llarp_buffer_t* buf) const;

      /// @param needBackup determines whether existing keys will be cycled
      bool
      EnsureKeys(const std::string& fpath, bool needBackup);

      bool
      KeyExchange(path_dh_func dh, SharedSecret& sharedkey,
                  const ServiceInfo& other, const KeyExchangeNonce& N) const;

      bool
      DecodeKey(const llarp_buffer_t& key, llarp_buffer_t* buf);

      nonstd::optional< EncryptedIntroSet >
      EncryptAndSignIntroSet(const IntroSet& i, llarp_time_t now) const;

      bool
      Sign(Signature& sig, const llarp_buffer_t& buf) const;
    };

    inline bool
    operator==(const Identity& lhs, const Identity& rhs)
    {
      return std::tie(lhs.enckey, lhs.signkey, lhs.pq, lhs.version, lhs.vanity)
          == std::tie(rhs.enckey, rhs.signkey, rhs.pq, rhs.version, rhs.vanity);
    }
  }  // namespace service
}  // namespace llarp

#endif
