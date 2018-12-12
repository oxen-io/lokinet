#ifndef LLARP_SERVICE_IDENTITY_HPP
#define LLARP_SERVICE_IDENTITY_HPP
#include <llarp/bencode.hpp>
#include <crypto.hpp>
#include <llarp/service/Info.hpp>
#include <llarp/service/IntroSet.hpp>
#include <llarp/service/types.hpp>

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
      KeyExchange(llarp::path_dh_func dh, byte_t* sharedkey,
                  const ServiceInfo& other, const byte_t* N) const;

      bool
      DecodeKey(llarp_buffer_t key, llarp_buffer_t* buf) override;

      bool
      SignIntroSet(IntroSet& i, llarp::Crypto* c, llarp_time_t now) const;

      bool
      Sign(llarp::Crypto*, byte_t* sig, llarp_buffer_t buf) const;
    };
  }  // namespace service
}  // namespace llarp

#endif
