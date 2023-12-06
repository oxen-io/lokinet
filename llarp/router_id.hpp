#pragma once

#include "util/aligned.hpp"
#include "util/status.hpp"

#include <llarp/crypto/types.hpp>

namespace llarp
{
  struct RouterID : public PubKey
  {
    static constexpr size_t SIZE = 32;

    using Data = std::array<byte_t, SIZE>;

    RouterID() = default;

    RouterID(const byte_t* buf) : PubKey(buf)
    {}

    RouterID(const Data& data) : PubKey(data)
    {}

    util::StatusObject
    ExtractStatus() const;

    std::string
    ToString() const;

    std::string
    ShortString() const;

    bool
    from_string(std::string_view str);

    RouterID&
    operator=(const byte_t* ptr)
    {
      std::copy(ptr, ptr + SIZE, begin());
      return *this;
    }
  };

  inline bool
  operator==(const RouterID& lhs, const RouterID& rhs)
  {
    return lhs.as_array() == rhs.as_array();
  }

  template <>
  constexpr inline bool IsToStringFormattable<RouterID> = true;
}  // namespace llarp

namespace std
{
  template <>
  struct hash<llarp::RouterID> : hash<llarp::AlignedBuffer<llarp::RouterID::SIZE>>
  {};
}  // namespace std
