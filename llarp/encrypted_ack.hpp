#ifndef LLARP_ENCRYPTED_ACK_HPP
#define LLARP_ENCRYPTED_ACK_HPP

#include <encrypted.hpp>

namespace llarp
{
  struct Crypto;

  struct EncryptedAck : public Encrypted
  {
    bool
    DecryptInPlace(const byte_t* symkey, const byte_t* nonce,
                   llarp::Crypto* crypto);
  };
}  // namespace llarp

#endif
