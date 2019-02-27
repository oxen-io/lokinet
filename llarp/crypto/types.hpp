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

  inline bool
  operator==(const PubKey &lhs, const PubKey &rhs)
  {
    return lhs.as_array() == rhs.as_array();
  }

  inline bool
  operator==(const PubKey &lhs, const RouterID &rhs)
  {
    return lhs.as_array() == rhs.as_array();
  }

  inline bool
  operator==(const RouterID &lhs, const PubKey &rhs)
  {
    return lhs.as_array() == rhs.as_array();
  }

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

    std::ostream &
    print(std::ostream &stream, int level, int spaces) const
    {
      Printer printer(stream, level, spaces);
      printer.printValue("secretkey");
      return stream;
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

  /// IdentitySecret is a secret key from a service node secret seed
  struct IdentitySecret final : public AlignedBuffer< 32 >
  {
    IdentitySecret() : AlignedBuffer< 32 >(){};

    /// no copy constructor
    explicit IdentitySecret(const IdentitySecret &) = delete;
    // no byte data constructor
    explicit IdentitySecret(const byte_t *) = delete;

    /// load service node seed from file
    bool
    LoadFromFile(const char *fname);
  };

  using ShortHash = AlignedBuffer< SHORTHASHSIZE >;
  struct Signature final : public AlignedBuffer< SIGSIZE >
  {
  };
  using TunnelNonce = AlignedBuffer< TUNNONCESIZE >;
  using SymmNonce   = AlignedBuffer< NONCESIZE >;
  using SymmKey     = AlignedBuffer< 32 >;

  using PQCipherBlock = AlignedBuffer< PQ_CIPHERTEXTSIZE + 1 >;
  using PQPubKey      = AlignedBuffer< PQ_PUBKEYSIZE >;
  using PQKeyPair     = AlignedBuffer< PQ_KEYPAIRSIZE >;
}  // namespace llarp

#endif
