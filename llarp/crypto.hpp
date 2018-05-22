#ifndef LLARP_CRYPTO_HPP
#define LLARP_CRYPTO_HPP

#include <llarp/crypto.h>
#include <array>

namespace llarp
{
  typedef std::array< uint8_t, sizeof(llarp_pubkey_t) > pubkey;
}

#endif
