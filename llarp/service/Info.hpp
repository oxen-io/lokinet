#ifndef LLARP_SERVICE_INFO_HPP
#define LLARP_SERVICE_INFO_HPP

#include <crypto/types.hpp>
#include <service/types.hpp>
#include <util/bencode.hpp>

#if __cplusplus >= 201703L
#include <optional>
#else
#include <tl/optional.hpp>
#endif

namespace llarp
{
  struct Crypto;

  namespace service
  {
    struct ServiceInfo final : public llarp::IBEncodeMessage
    {
     private:
      llarp::PubKey enckey;
      llarp::PubKey signkey;

     public:
      VanityNonce vanity;

#if __cplusplus >= 201703L
      using OptNonce = std::optional< VanityNonce >;
#else
      using OptNonce = tl::optional< VanityNonce >;
#endif

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
      Verify(llarp::Crypto* crypto, llarp_buffer_t payload,
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
      };

      bool
      operator<(const ServiceInfo& other) const
      {
        return Addr() < other.Addr();
      }

      friend std::ostream&
      operator<<(std::ostream& out, const ServiceInfo& i)
      {
        return out << "[e=" << i.enckey << " s=" << i.signkey
                   << " v=" << i.version << " x=" << i.vanity << "]";
      }

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
      DecodeKey(llarp_buffer_t key, llarp_buffer_t* buf) override;

     private:
      Address m_CachedAddr;
    };
  }  // namespace service
}  // namespace llarp

#endif
