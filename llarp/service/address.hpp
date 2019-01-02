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
    struct Address : public AlignedBuffer< 32 >
    {
      std::string
      ToString(const char* tld = ".loki") const;

      bool
      FromString(const std::string& str, const char* tld = ".loki");

      Address() : AlignedBuffer< SIZE >()
      {
      }

      explicit Address(const Data& buf) : AlignedBuffer< SIZE >(buf)
      {
      }

      Address(const Address& other) : AlignedBuffer< SIZE >(other.as_array())
      {
      }

      explicit Address(const AlignedBuffer< SIZE >& other)
          : AlignedBuffer< SIZE >(other)
      {
      }

      bool
      operator<(const Address& other) const
      {
        return as_array() < other.as_array();
      }

      friend std::ostream&
      operator<<(std::ostream& out, const Address& self)
      {
        return out << self.ToString();
      }

      bool
      operator==(const Address& other) const
      {
        return as_array() == other.as_array();
      }

      bool
      operator!=(const Address& other) const
      {
        return as_array() != other.as_array();
      }

      Address&
      operator=(const Address& other) = default;

      dht::Key_t
      ToKey() const
      {
        return dht::Key_t(as_array());
      }

      RouterID
      ToRouter() const
      {
        return RouterID(as_array());
      }

      struct Hash
      {
        size_t
        operator()(const Address& buf) const
        {
          return std::accumulate(buf.begin(), buf.end(), 0,
                                 std::bit_xor< size_t >());
        }
      };
    };

  }  // namespace service
}  // namespace llarp

#endif
