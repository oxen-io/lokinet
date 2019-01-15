#ifndef LLARP_CRYPTO_TYPES_HPP
#define LLARP_CRYPTO_TYPES_HPP

#include <crypto/constants.hpp>
#include <router_id.hpp>
#include <util/aligned.hpp>
#include <util/types.hpp>

#include <algorithm>
#include <iostream>

namespace llarp
{
  using SharedSecret     = AlignedBuffer< SHAREDKEYSIZE >;
  using KeyExchangeNonce = AlignedBuffer< 32 >;

  struct PubKey final : public AlignedBuffer< PUBKEYSIZE >
  {
    PubKey() : AlignedBuffer< SIZE >()
    {
    }

    explicit PubKey(const byte_t *ptr) : AlignedBuffer< SIZE >(ptr)
    {
    }

    explicit PubKey(const Data &data) : AlignedBuffer< SIZE >(data)
    {
    }

    explicit PubKey(const AlignedBuffer< SIZE > &other)
        : AlignedBuffer< SIZE >(other)
    {
    }

    std::string
    ToString() const;

    bool
    FromString(const std::string &str);

    friend std::ostream &
    operator<<(std::ostream &out, const PubKey &k)
    {
      return out << k.ToString();
    }

    operator RouterID() const
    {
      return RouterID(as_array());
    }

    PubKey &
    operator=(const byte_t *ptr)
    {
      std::copy(ptr, ptr + SIZE, begin());
      return *this;
    }
  };

  struct SecretKey final : public AlignedBuffer< SECKEYSIZE >
  {
    SecretKey() : AlignedBuffer< SECKEYSIZE >(){};

    explicit SecretKey(const SecretKey &k) : AlignedBuffer< SECKEYSIZE >(k)
    {
    }

    explicit SecretKey(const byte_t *ptr) : AlignedBuffer< SECKEYSIZE >(ptr)
    {
    }

    friend std::ostream &
    operator<<(std::ostream &out, const SecretKey &)
    {
      // make sure we never print out secret keys
      return out << "[secretkey]";
    }

    PubKey
    toPublic() const
    {
      return PubKey(data() + 32);
    }

    bool
    LoadFromFile(const char *fname);

    bool
    SaveToFile(const char *fname) const;

    SecretKey &
    operator=(const byte_t *ptr)
    {
      std::copy(ptr, ptr + SIZE, begin());
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
