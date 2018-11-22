#ifndef LLARP_CRYPTO_HPP
#define LLARP_CRYPTO_HPP

#include <llarp/crypto.h>
#include <llarp/mem.h>
#include <llarp/threadpool.h>
#include <llarp/aligned.hpp>

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
  using PubKey           = AlignedBuffer< PUBKEYSIZE >;
  using SecretKey        = AlignedBuffer< SECKEYSIZE >;
  using ShortHash        = AlignedBuffer< SHORTHASHSIZE >;
  using Signature        = AlignedBuffer< SIGSIZE >;
  using TunnelNonce      = AlignedBuffer< TUNNONCESIZE >;
  using SymmNonce        = AlignedBuffer< NONCESIZE >;
  using SymmKey          = AlignedBuffer< 32 >;

  using PQCipherBlock = AlignedBuffer< PQ_CIPHERTEXTSIZE + 1 >;
  using PQPubKey      = AlignedBuffer< PQ_PUBKEYSIZE >;
  using PQKeyPair     = AlignedBuffer< PQ_KEYPAIRSIZE >;

}  // namespace llarp

#endif
