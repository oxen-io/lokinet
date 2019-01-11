#ifndef LLARP_SERVICE_IDENTITY_HPP
#define LLARP_SERVICE_IDENTITY_HPP

#include <crypto.hpp>
#include <service/Info.hpp>
#include <service/IntroSet.hpp>
#include <service/types.hpp>
#include <util/bencode.hpp>

namespace llarp
{
  namespace service
  {
    // private keys
    struct Identity final : public llarp::IBEncodeMessage
    {
      llarp::SecretKey enckey;
      llarp::SecretKey signkey;
      llarp::PQKeyPair pq;
      uint64_t version = 0;
      VanityNonce vanity;

      // public service info
      ServiceInfo pub;

      ~Identity();

      // regenerate secret keys
      void
      RegenerateKeys(llarp::Crypto* c);

      // load from file
      bool
      LoadFromFile(const std::string& fpath);

      bool
      BEncode(llarp_buffer_t* buf) const override;

      bool
      EnsureKeys(const std::string& fpath, llarp::Crypto* c);

      bool
      KeyExchange(llarp::path_dh_func dh, SharedSecret& sharedkey,
                  const ServiceInfo& other, const KeyExchangeNonce& N) const;

      bool
      DecodeKey(llarp_buffer_t key, llarp_buffer_t* buf) override;

      bool
      SignIntroSet(IntroSet& i, llarp::Crypto* c, llarp_time_t now) const;

      bool
      Sign(llarp::Crypto*, Signature& sig, llarp_buffer_t buf) const;
    };
  }  // namespace service
}  // namespace llarp

#endif
