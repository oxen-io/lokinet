#pragma once
#include <llarp/crypto/types.hpp>
#include "address.hpp"

namespace llarp::service
{
  struct EncryptedName
  {
    SymmNonce nonce;
    std::string ciphertext;

    std::optional<Address>
    Decrypt(std::string_view name) const;
  };

  /// check if an lns name complies with the registration rules
  bool
  NameIsValid(std::string_view name);

}  // namespace llarp::service
