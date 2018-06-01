#ifndef LLARP_CRYPTO_HPP
#define LLARP_CRYPTO_HPP

#include <llarp/crypto.h>
#include <llarp/aligned.hpp>

namespace llarp
{
  typedef AlignedBuffer< PUBKEYSIZE > pubkey;

  struct pubkeyhash
  {
    std::size_t
    operator()(pubkey const& a) const noexcept
    {
      size_t sz = 0;
      memcpy(&sz, a.data(), sizeof(size_t));
      return sz;
    }
  };
}

#endif
