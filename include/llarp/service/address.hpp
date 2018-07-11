#ifndef LLARP_SERVICE_ADDRESS_HPP
#define LLARP_SERVICE_ADDRESS_HPP
#include <llarp/aligned.hpp>
#include <string>

namespace llarp
{
  namespace service
  {
    typedef llarp::AlignedBuffer< 32 > Address;

    std::string
    AddressToString(const Address& addr);
  }
}

#endif