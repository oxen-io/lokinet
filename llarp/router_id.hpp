#ifndef LLARP_ROUTER_ID_HPP
#define LLARP_ROUTER_ID_HPP

#include <aligned.hpp>

namespace llarp
{
  struct RouterID : public AlignedBuffer< 32 >
  {
    static constexpr size_t SIZE = 32;

    using Data = std::array< byte_t, SIZE >;

    RouterID() : AlignedBuffer< SIZE >()
    {
    }

    RouterID(const byte_t* buf) : AlignedBuffer< SIZE >(buf)
    {
    }

    RouterID(const Data& data) : AlignedBuffer< SIZE >(data)
    {
    }

    std::string
    ToString() const;

    bool
    FromString(const std::string& str);

    RouterID&
    operator=(const byte_t* ptr)
    {
      memcpy(data(), ptr, SIZE);
      return *this;
    }

    friend std::ostream&
    operator<<(std::ostream& out, const RouterID& id)
    {
      return out << id.ToString();
    }

    using Hash = AlignedBuffer< SIZE >::Hash;
  };
}  // namespace llarp

#endif
