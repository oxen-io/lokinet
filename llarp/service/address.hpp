#ifndef LLARP_SERVICE_ADDRESS_HPP
#define LLARP_SERVICE_ADDRESS_HPP
#include <llarp/aligned.hpp>
#include <dht/key.hpp>
#include <llarp/router_id.hpp>
#include <string>

namespace llarp
{
  namespace service
  {
    /// Snapp/Snode Address
    struct Address
    {
      std::string
      ToString(const char* tld = ".loki") const;

      bool
      FromString(const std::string& str, const char* tld = ".loki");

      Address()
      {
        Zero();
      }

      Address(const byte_t* buf)
      {
        memcpy(b, buf, 32);
      }

      Address(const Address& other)
      {
        memcpy(b, other.b, 32);
      }

      byte_t& operator[](size_t idx)
      {
        return b[idx];
      }

      const byte_t& operator[](size_t idx) const
      {
        return b[idx];
      }

      bool
      BEncode(llarp_buffer_t* buf) const
      {
        return bencode_write_bytestring(buf, b, 32);
      }

      bool
      BDecode(llarp_buffer_t* buf)
      {
        llarp_buffer_t strbuf;
        if(!bencode_read_string(buf, &strbuf))
          return false;
        if(strbuf.sz != 32)
        {
          llarp::LogErrorTag("Address::BDecode",
                             "bdecode buffer size missmatch ", strbuf.sz,
                             "!=32");
          return false;
        }
        memcpy(b, strbuf.base, 32);
        return true;
      }

      size_t
      size() const
      {
        return 32;
      }

      bool
      IsZero() const
      {
        size_t sz = 4;
        while(sz)
        {
          if(l[--sz])
            return false;
        }
        return true;
      }

      void
      Zero()
      {
        size_t sz = 4;
        while(sz)
        {
          l[--sz] = 0;
        }
      }

      bool
      operator<(const Address& other) const
      {
        return memcmp(data(), other.data(), 32) < 0;
      }

      friend std::ostream&
      operator<<(std::ostream& out, const Address& self)
      {
        return out << self.ToString();
      }

      bool
      operator==(const Address& other) const
      {
        return memcmp(data(), other.data(), 32) == 0;
      }

      bool
      operator!=(const Address& other) const
      {
        return !(*this == other);
      }

      Address&
      operator=(const Address& other)
      {
        memcpy(data(), other.data(), 32);
        return *this;
      }

      const dht::Key_t
      ToKey() const
      {
        return dht::Key_t(data());
      }

      const RouterID
      ToRouter() const
      {
        return RouterID(data());
      }

      const byte_t*
      data() const
      {
        return b;
      }

      byte_t*
      data()
      {
        return b;
      }

      const uint64_t*
      data_l() const
      {
        return l;
      }

      struct Hash
      {
        size_t
        operator()(const Address& buf) const
        {
          return *buf.data_l();
        }
      };

     private:
      union {
        byte_t b[32];
        uint64_t l[4];
      };
    };

  }  // namespace service
}  // namespace llarp

#endif
