#ifndef LLARP_POW_HPP
#define LLARP_POW_HPP
#include <llarp/crypto.h>
#include <llarp/bencode.hpp>
#include <llarp/router_id.hpp>

namespace llarp
{
  /// proof of work
  struct PoW : public IBEncodeMessage
  {
    static constexpr size_t MaxSize = 128;
    uint64_t timestamp              = 0;
    uint32_t extendedLifetime       = 0;
    AlignedBuffer< 32 > nonce;

    ~PoW();

    bool
    IsValid(llarp_shorthash_func hashfunc) const;

    bool
    BDecode(llarp_buffer_t* buf);

    bool
    DecodeKey(llarp_buffer_t k, llarp_buffer_t* val);

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

    friend std::ostream&
    operator<<(std::ostream& out, const PoW& p)
    {
      return out << "[pow timestamp=" << p.timestamp
                 << " lifetime=" << p.extendedLifetime << " nonce=" << p.nonce
                 << "]";
    }
  };
}  // namespace llarp

#endif