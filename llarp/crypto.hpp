#ifndef LLARP_CRYPTO_HPP
#define LLARP_CRYPTO_HPP

#include <aligned.hpp>
#include <crypto.h>
#include <mem.h>
#include <router_id.hpp>
#include <threadpool.h>

namespace llarp
{
  const byte_t*
  seckey_topublic(const byte_t* secret);

  const byte_t*
  pq_keypair_to_public(const byte_t* keypair);

  const byte_t*
  pq_keypair_to_secret(const byte_t* keypair);

  using SharedSecret     = AlignedBuffer< SHAREDKEYSIZE >;
  using KeyExchangeNonce = AlignedBuffer< 32 >;

  struct PubKey final : public AlignedBuffer< PUBKEYSIZE >
  {
    PubKey() : AlignedBuffer< PUBKEYSIZE >(){};
    PubKey(const byte_t* ptr) : AlignedBuffer< PUBKEYSIZE >(ptr){};

    std::string
    ToString() const;

    bool
    FromString(const std::string& str);

    friend std::ostream&
    operator<<(std::ostream& out, const PubKey& k)
    {
      return out << k.ToString();
    }

    operator RouterID() const
    {
      return RouterID(data());
    }

    PubKey&
    operator=(const byte_t* ptr)
    {
      memcpy(data(), ptr, size());
      return *this;
    }
  };

  struct SecretKey final : public AlignedBuffer< SECKEYSIZE >
  {
    friend std::ostream&
    operator<<(std::ostream& out, const SecretKey&)
    {
      // make sure we never print out secret keys
      return out << "[secretkey]";
    }

    SecretKey&
    operator=(const byte_t* ptr)
    {
      memcpy(data(), ptr, size());
      return *this;
    }
  };

  using ShortHash   = AlignedBuffer< SHORTHASHSIZE >;
  using Signature   = AlignedBuffer< SIGSIZE >;
  using TunnelNonce = AlignedBuffer< TUNNONCESIZE >;
  using SymmNonce   = AlignedBuffer< NONCESIZE >;
  using SymmKey     = AlignedBuffer< 32 >;

  using PQCipherBlock = AlignedBuffer< PQ_CIPHERTEXTSIZE + 1 >;
  using PQPubKey      = AlignedBuffer< PQ_PUBKEYSIZE >;
  using PQKeyPair     = AlignedBuffer< PQ_KEYPAIRSIZE >;

}  // namespace llarp

#endif
