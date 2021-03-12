#include "convotag.hpp"
#include "net/ip.hpp"

namespace llarp::service
{
  void
  ConvoTag::Randomize()
  {
    llarp::AlignedBuffer<16>::Randomize();
    /// ensure we are in the fc00 range
    llarp::AlignedBuffer<16>::operator[](0) = 0xfc;
  }

  sockaddr_in6
  ConvoTag::ToV6() const
  {
    const auto* ptr = reinterpret_cast<const uint64_t*>(data());
    sockaddr_in6 saddr{};
    saddr.sin6_family = AF_INET6;
    saddr.sin6_addr = net::HUIntToIn6(huint128_t{uint128_t{ptr[0], ptr[1]}});
    return saddr;
  }

  void
  ConvoTag::FromV6(sockaddr_in6 saddr)
  {
    std::copy_n(saddr.sin6_addr.s6_addr, size(), data());
  }

}  // namespace llarp::service
