#ifndef LLARP_SERVICE_IDENTITY_HPP
#define LLARP_SERVICE_IDENTITY_HPP

#include <crypto/types.hpp>
#include <service/info.hpp>
#include <service/intro_set.hpp>
#include <service/vanity.hpp>
#include <util/bencode.hpp>

namespace llarp
{
  struct Crypto;

  namespace service
  {
    // private keys
    struct Identity final : public IBEncodeMessage
    {
      SecretKey enckey;
      SecretKey signkey;
      PQKeyPair pq;
      uint64_t version = 0;
      VanityNonce vanity;

      // public service info
      ServiceInfo pub;

      ~Identity();

      // regenerate secret keys
      void
      RegenerateKeys(Crypto* c);

      // load from file
      bool
      LoadFromFile(const std::string& fpath);

      bool
      BEncode(llarp_buffer_t* buf) const override;

      bool
      EnsureKeys(const std::string& fpath, Crypto* c);

      bool
      KeyExchange(path_dh_func dh, SharedSecret& sharedkey,
                  const ServiceInfo& other, const KeyExchangeNonce& N) const;

      bool
      DecodeKey(const llarp_buffer_t& key, llarp_buffer_t* buf) override;

      bool
      SignIntroSet(IntroSet& i, Crypto* c, llarp_time_t now) const;

      bool
      Sign(Crypto*, Signature& sig, const llarp_buffer_t& buf) const;
    };
  }  // namespace service
}  // namespace llarp

#endif
