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

  typedef AlignedBuffer< SHAREDKEYSIZE > SharedSecret;
  typedef AlignedBuffer< 32 > KeyExchangeNonce;
  typedef AlignedBuffer< PUBKEYSIZE > PubKey;
  typedef AlignedBuffer< SECKEYSIZE > SecretKey;
  typedef AlignedBuffer< SHORTHASHSIZE > ShortHash;
  typedef AlignedBuffer< SIGSIZE > Signature;
  typedef AlignedBuffer< TUNNONCESIZE > TunnelNonce;
  typedef AlignedBuffer< NONCESIZE > SymmNonce;
  typedef AlignedBuffer< 32 > SymmKey;

  typedef AlignedBuffer< PQ_CIPHERTEXTSIZE + 1 > PQCipherBlock;
  typedef AlignedBuffer< PQ_PUBKEYSIZE > PQPubKey;
  typedef AlignedBuffer< PQ_KEYPAIRSIZE > PQKeyPair;

}  // namespace llarp

#endif
