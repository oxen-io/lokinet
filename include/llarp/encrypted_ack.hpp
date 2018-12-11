#ifndef LLARP_ENCRYPTED_ACK_HPP
#define LLARP_ENCRYPTED_ACK_HPP
#include <llarp/encrypted.hpp>
namespace llarp
{
  struct EncryptedAck : public Encrypted
  {
    bool
    DecryptInPlace(const byte_t* symkey, const byte_t* nonce,
                   llarp::Crypto* crypto);
  };
}  // namespace llarp

#endif
