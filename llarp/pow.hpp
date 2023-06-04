#pragma once

#include "router_id.hpp"
#include "util/buffer.hpp"

namespace llarp
{
  /// proof of work
  struct PoW
  {
    static constexpr size_t MaxSize = 128;
    llarp_time_t timestamp = 0s;
    llarp_time_t extendedLifetime = 0s;
    AlignedBuffer<32> nonce;
    uint64_t version = llarp::constants::proto_version;

    ~PoW();

    bool
    IsValid(llarp_time_t now) const;

    bool
    DecodeKey(const llarp_buffer_t& k, llarp_buffer_t* val);

    bool
    BEncode(llarp_buffer_t* buf) const;

    bool
    operator==(const PoW& other) const
    {
      return timestamp == other.timestamp && version == other.version
          && extendedLifetime == other.extendedLifetime && nonce == other.nonce;
    }

    bool
    operator!=(const PoW& other) const
    {
      return !(*this == other);
    }

    std::string
    ToString() const;
  };

  template <>
  constexpr inline bool IsToStringFormattable<PoW> = true;

}  // namespace llarp
