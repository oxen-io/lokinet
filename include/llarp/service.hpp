#ifndef LLARP_SERVICE_HPP
#define LLARP_SERVICE_HPP
#include <llarp/aligned.hpp>
#include <llarp/bencode.hpp>
#include <llarp/crypto.hpp>
#include <llarp/path_types.hpp>

#include <set>

namespace llarp
{
  namespace service
  {
    /// hidden service address
    typedef llarp::AlignedBuffer< 32 > Address;

    typedef llarp::AlignedBuffer< 16 > VanityNonce;

    struct ServiceInfo : public llarp::IBEncodeMessage
    {
      llarp::PubKey enckey;
      llarp::PubKey signkey;
      uint64_t version = 0;
      VanityNonce vanity;

      ~ServiceInfo();

      /// calculate our address
      void
      CalculateAddress(Address& addr) const;

      bool
      BEncode(llarp_buffer_t* buf) const;

      bool
      DecodeKey(llarp_buffer_t key, llarp_buffer_t* buf);
    };

    // private keys
    struct Identity : public llarp::IBEncodeMessage
    {
      llarp::SecretKey enckey;
      llarp::SecretKey signkey;
      uint64_t version = 0;
      VanityNonce vanity;

      // public service info
      ServiceInfo pub;

      // regenerate secret keys
      void
      RegenerateKeys(llarp_crypto* c);

      // load from file
      bool
      LoadFromFile(const std::string& fpath);

      bool
      BEncode(llarp_buffer_t* buf) const;

      bool
      DecodeKey(llarp_buffer_t key, llarp_buffer_t* buf);
    };

    struct Introduction : public llarp::IBEncodeMessage
    {
      llarp::PubKey router;
      llarp::PathID_t pathID;
      uint64_t version = 0;
      uint64_t expiresAt;

      ~Introduction()
      {
      }

      bool
      BEncode(llarp_buffer_t* buf) const;

      bool
      DecodeKey(llarp_buffer_t key, llarp_buffer_t* buf);
    };

    struct IntroSet : public llarp::IBEncodeMessage
    {
      ServiceInfo A;
      std::set< Introduction > I;
      llarp::Signature Z;

      ~IntroSet();

      bool
      BDecode(llarp_buffer_t* buf);

      bool
      BEncode(llarp_buffer_t* buf) const;

      bool
      DecodeKey(llarp_buffer_t key, llarp_buffer_t* buf);
    };

  };  // namespace service
}  // namespace llarp

#endif