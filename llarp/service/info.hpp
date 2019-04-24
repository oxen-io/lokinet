#ifndef LLARP_SERVICE_INFO_HPP
#define LLARP_SERVICE_INFO_HPP

#include <crypto/types.hpp>
#include <service/address.hpp>
#include <service/vanity.hpp>
#include <util/bencode.hpp>

#include <absl/types/optional.h>

namespace llarp
{
  struct Crypto;

  namespace service
  {
    struct ServiceInfo final : public IBEncodeMessage
    {
     private:
      PubKey enckey;
      PubKey signkey;

     public:
      VanityNonce vanity;

      using OptNonce = absl::optional< VanityNonce >;

      ServiceInfo() = default;

      ServiceInfo(ServiceInfo&& other)
      {
        enckey       = std::move(other.enckey);
        signkey      = std::move(other.signkey);
        version      = std::move(other.version);
        vanity       = std::move(other.vanity);
        m_CachedAddr = std::move(other.m_CachedAddr);
      }

      ServiceInfo(const ServiceInfo& other)
          : IBEncodeMessage(other.version)
          , enckey(other.enckey)
          , signkey(other.signkey)
          , vanity(other.vanity)
          , m_CachedAddr(other.m_CachedAddr)
      {
        version = other.version;
      }

      void
      RandomizeVanity()
      {
        vanity.Randomize();
      }

      bool
      Verify(Crypto* crypto, const llarp_buffer_t& payload,
             const Signature& sig) const;

      const PubKey&
      EncryptionPublicKey() const
      {
        return enckey;
      }

      bool
      Update(const byte_t* enc, const byte_t* sign,
             const OptNonce& nonce = OptNonce())
      {
        enckey  = enc;
        signkey = sign;
        if(nonce)
        {
          vanity = nonce.value();
        }
        return UpdateAddr();
      }

      bool
      operator==(const ServiceInfo& other) const
      {
        return enckey == other.enckey && signkey == other.signkey
            && version == other.version && vanity == other.vanity;
      }

      bool
      operator!=(const ServiceInfo& other) const
      {
        return !(*this == other);
      }

      ServiceInfo&
      operator=(const ServiceInfo& other)
      {
        enckey  = other.enckey;
        signkey = other.signkey;
        version = other.version;
        vanity  = other.vanity;
        version = other.version;
        UpdateAddr();
        return *this;
      }

      bool
      operator<(const ServiceInfo& other) const
      {
        return Addr() < other.Addr();
      }

      std::ostream&
      print(std::ostream& stream, int level, int spaces) const;

      /// .loki address
      std::string
      Name() const;

      bool
      UpdateAddr();

      const Address&
      Addr() const
      {
        return m_CachedAddr;
      }

      /// calculate our address
      bool CalculateAddress(std::array< byte_t, 32 >& data) const;

      bool
      BDecode(llarp_buffer_t* buf) override
      {
        if(IBEncodeMessage::BDecode(buf))
          return CalculateAddress(m_CachedAddr.as_array());
        return false;
      }

      bool
      BEncode(llarp_buffer_t* buf) const override;

      bool
      DecodeKey(const llarp_buffer_t& key, llarp_buffer_t* buf) override;

     private:
      Address m_CachedAddr;
    };

    inline std::ostream&
    operator<<(std::ostream& out, const ServiceInfo& i)
    {
      return i.print(out, -1, -1);
    }
  }  // namespace service
}  // namespace llarp

#endif
