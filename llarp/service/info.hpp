#pragma once

#include <llarp/crypto/types.hpp>
#include "address.hpp"
#include "vanity.hpp"
#include <llarp/util/bencode.hpp>

#include <optional>

namespace llarp
{
  namespace service
  {
    struct ServiceInfo
    {
     private:
      PubKey enckey;
      PubKey signkey;
      mutable Address m_CachedAddr;

     public:
      VanityNonce vanity;
      uint64_t version = llarp::constants::proto_version;

      void
      RandomizeVanity()
      {
        vanity.Randomize();
      }

      bool
      Verify(const llarp_buffer_t& payload, const Signature& sig) const;

      const PubKey&
      EncryptionPublicKey() const
      {
        if (m_CachedAddr.IsZero())
        {
          CalculateAddress(m_CachedAddr.as_array());
        }
        return enckey;
      }

      bool
      Update(const byte_t* sign, const byte_t* enc, const std::optional<VanityNonce>& nonce = {});

      bool
      operator==(const ServiceInfo& other) const
      {
        return enckey == other.enckey && signkey == other.signkey && version == other.version
            && vanity == other.vanity;
      }

      bool
      operator!=(const ServiceInfo& other) const
      {
        return !(*this == other);
      }

      bool
      operator<(const ServiceInfo& other) const
      {
        return Addr() < other.Addr();
      }

      std::string
      ToString() const;

      /// .loki address
      std::string
      Name() const;

      bool
      UpdateAddr();

      const Address&
      Addr() const
      {
        if (m_CachedAddr.IsZero())
        {
          CalculateAddress(m_CachedAddr.as_array());
        }
        return m_CachedAddr;
      }

      /// calculate our address
      bool
      CalculateAddress(std::array<byte_t, 32>& data) const;

      bool
      BDecode(llarp_buffer_t* buf)
      {
        if (not bencode_decode_dict(*this, buf))
          return false;
        return UpdateAddr();
      }

      bool
      BEncode(llarp_buffer_t* buf) const;

      bool
      DecodeKey(const llarp_buffer_t& key, llarp_buffer_t* buf);
    };
  }  // namespace service
}  // namespace llarp

template <>
constexpr inline bool llarp::IsToStringFormattable<llarp::service::ServiceInfo> = true;
