#pragma once
#include "address.hpp"

#include <llarp/crypto/types.hpp>

namespace llarp::service
{
    struct EncryptedName
    {
        SymmNonce nonce;
        std::string ciphertext;

        std::optional<Address> Decrypt(std::string_view name) const;
    };

    /// check if an lns name complies with the registration rules
    bool is_valid_name(std::string_view name);

}  // namespace llarp::service
