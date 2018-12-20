#ifndef LLARP_SERVICE_ADDRESS_HPP
#define LLARP_SERVICE_ADDRESS_HPP

#include <aligned.hpp>
#include <dht/key.hpp>
#include <router_id.hpp>

#include <functional>
#include <numeric>
#include <string>

namespace llarp
{
  namespace service
  {
    /// Snapp/Snode Address
    struct Address
    {
      static constexpr size_t SIZE = 32;

      using Data = std::array< byte_t, SIZE >;

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
        std::copy(buf, buf + SIZE, b.begin());
      }

      Address(const Address& other)
      {
        b = other.b;
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
        return bencode_write_bytestring(buf, b.data(), SIZE);
      }

      bool
      BDecode(llarp_buffer_t* buf)
      {
        llarp_buffer_t strbuf;
        if(!bencode_read_string(buf, &strbuf))
          return false;
        if(strbuf.sz != SIZE)
        {
          llarp::LogErrorTag("Address::BDecode",
                             "bdecode buffer size missmatch ", strbuf.sz,
                             "!=32");
          return false;
        }
        std::copy(strbuf.base, strbuf.base + SIZE, b.begin());
        return true;
      }

      static constexpr size_t
      size()
      {
        return SIZE;
      }

      bool
      IsZero() const
      {
        return b == Data{};
      }

      void
      Zero()
      {
        b.fill(0);
      }

      bool
      operator<(const Address& other) const
      {
        return data() < other.data();
      }

      friend std::ostream&
      operator<<(std::ostream& out, const Address& self)
      {
        return out << self.ToString();
      }

      bool
      operator==(const Address& other) const
      {
        return data() == other.data();
      }

      bool
      operator!=(const Address& other) const
      {
        return !(*this == other);
      }

      Address&
      operator=(const Address& other) = default;

      const dht::Key_t
      ToKey() const
      {
        return dht::Key_t(data());
      }

      const RouterID
      ToRouter() const
      {
        return RouterID(data().data());
      }

      const Data&
      data() const
      {
        return b;
      }

      Data&
      data()
      {
        return b;
      }

      struct Hash
      {
        size_t
        operator()(const Address& buf) const
        {
          return std::accumulate(buf.data().begin(), buf.data().end(), 0,
                                 std::bit_xor< size_t >());
        }
      };

     private:
      Data b;
    };

  }  // namespace service
}  // namespace llarp

#endif
