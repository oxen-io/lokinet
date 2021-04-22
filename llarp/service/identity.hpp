#pragma once

#include <llarp/config/key_manager.hpp>
#include <llarp/constants/proto.hpp>
#include <llarp/crypto/types.hpp>
#include <memory>
#include "info.hpp"
#include "intro_set.hpp"
#include "vanity.hpp"
#include <llarp/util/buffer.hpp>

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
      void
      EnsureKeys(fs::path fpath, bool needBackup);

      bool
      KeyExchange(
          path_dh_func dh,
          SharedSecret& sharedkey,
          const ServiceInfo& other,
          const KeyExchangeNonce& N) const;

      bool
      DecodeKey(const llarp_buffer_t& key, llarp_buffer_t* buf);

      std::optional<EncryptedIntroSet>
      EncryptAndSignIntroSet(const IntroSet& i, llarp_time_t now) const;

      bool
      Sign(Signature& sig, const llarp_buffer_t& buf) const;

      /// zero out all secret key members
      void
      Clear();
    };

    inline bool
    operator==(const Identity& lhs, const Identity& rhs)
    {
      return std::tie(lhs.enckey, lhs.signkey, lhs.pq, lhs.version, lhs.vanity)
          == std::tie(rhs.enckey, rhs.signkey, rhs.pq, rhs.version, rhs.vanity);
    }
  }  // namespace service
}  // namespace llarp
