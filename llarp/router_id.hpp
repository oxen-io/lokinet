#ifndef LLARP_ROUTER_ID_HPP
#define LLARP_ROUTER_ID_HPP

#include <aligned.hpp>

namespace llarp
{
  struct RouterID : public AlignedBuffer< 32 >
  {
    RouterID() : AlignedBuffer< 32 >()
    {
    }

    RouterID(const byte_t* buf) : AlignedBuffer< 32 >(buf)
    {
    }

    std::string
    ToString() const;

    bool
    FromString(const std::string& str);

    RouterID&
    operator=(const byte_t* ptr)
    {
      memcpy(data(), ptr, 32);
      return *this;
    }

    friend std::ostream&
    operator<<(std::ostream& out, const RouterID& id)
    {
      return out << id.ToString();
    }

    using Hash = AlignedBuffer< 32 >::Hash;
  };
}  // namespace llarp

#endif
