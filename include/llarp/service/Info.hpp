#ifndef LLARP_SERVICE_INFO_HPP
#define LLARP_SERVICE_INFO_HPP
#include <llarp/bencode.hpp>
#include <llarp/crypto.hpp>
#include <llarp/service/types.hpp>

namespace llarp
{
  namespace service
  {
    struct ServiceInfo : public llarp::IBEncodeMessage
    {
      llarp::PubKey enckey;
      llarp::PubKey signkey;
      uint64_t version = 0;
      VanityNonce vanity;

      ServiceInfo();

      ~ServiceInfo();

      bool
      operator==(const ServiceInfo& other) const
      {
        return enckey == other.enckey && signkey == other.signkey
            && version == other.version && vanity == other.vanity;
      }

      ServiceInfo&
      operator=(const ServiceInfo& other)
      {
        enckey  = other.enckey;
        signkey = other.signkey;
        version = other.version;
        vanity  = other.vanity;
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

      /// compute .loki address
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
      BDecode(llarp_buffer_t* buf)
      {
        if(!IBEncodeMessage::BDecode(buf))
          return false;
        return CalculateAddress(m_CachedAddr);
      }

      bool
      BEncode(llarp_buffer_t* buf) const;

      bool
      DecodeKey(llarp_buffer_t key, llarp_buffer_t* buf);

     private:
      Address m_CachedAddr;
    };
  }  // namespace service
}  // namespace llarp

#endif