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

      Address() : llarp::AlignedBuffer< 32 >()
      {
      }

      Address(const byte_t* data) : llarp::AlignedBuffer< 32 >(data)
      {
      }
    };

  }  // namespace service
}  // namespace llarp

#endif