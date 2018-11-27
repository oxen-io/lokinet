#ifndef LLARP_SERVICE_INFO_HPP
#define LLARP_SERVICE_INFO_HPP
#include <llarp/bencode.hpp>
#include <llarp/crypto.hpp>
#include <llarp/service/types.hpp>

namespace llarp
{
  namespace service
  {
    struct ServiceInfo final : public llarp::IBEncodeMessage
    {
     private:
      llarp::PubKey enckey;
      llarp::PubKey signkey;
    
     public:
      VanityNonce vanity;
      
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
      Verify(llarp_crypto* crypto, llarp_buffer_t payload,
             const Signature& sig) const
      {
        return crypto->verify(signkey, payload, sig);
      }

      const byte_t*
      EncryptionPublicKey() const
      {
        return enckey;
      }

      bool
      Update(const byte_t* enc, const byte_t* sign, const byte_t * nonce=nullptr)
      {
        enckey  = enc;
        signkey = sign;
        if(nonce)
          vanity = nonce;
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
      bool
      CalculateAddress(byte_t* buf) const;

      bool
      BDecode(llarp_buffer_t* buf) override
      {
        if(IBEncodeMessage::BDecode(buf))
          return CalculateAddress(m_CachedAddr.data());
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
