#ifndef LLARP_CRYPTO_HPP
#define LLARP_CRYPTO_HPP

#include <array>
#include <llarp/crypto.h>

namespace llarp
{
  typedef std::array<uint8_t, sizeof(llarp_pubkey_t)> pubkey;
}

#endif
