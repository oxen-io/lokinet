#pragma once
#include <crypto/types.hpp>
#include <service/address.hpp>

namespace llarp::service
{
  struct EncryptedName
  {
    SymmNonce nonce;
    std::string ciphertext;

    std::optional<Address>
    Decrypt(std::string_view name) const;
  };
}  // namespace llarp::service
