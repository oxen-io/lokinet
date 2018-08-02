#ifndef LLARP_SERVICE_ADDRESS_HPP
#define LLARP_SERVICE_ADDRESS_HPP
#include <llarp/aligned.hpp>
#include <llarp/dht/key.hpp>
#include <string>

namespace llarp
{
  namespace service
  {
    struct Address : public llarp::AlignedBuffer< 32 >
    {
      std::string
      ToString() const;

      bool
      FromString(const std::string& str);

      Address() : llarp::AlignedBuffer< 32 >()
      {
      }

      Address(const byte_t* data) : llarp::AlignedBuffer< 32 >(data)
      {
      }
      struct Hash
      {
        size_t
        operator()(const Address& addr) const
        {
          size_t idx = 0;
          memcpy(&idx, addr, sizeof(idx));
          return idx;
        }
      };

      operator const dht::Key_t() const
      {
        return dht::Key_t(data());
      }
    };

  }  // namespace service
}  // namespace llarp

#endif