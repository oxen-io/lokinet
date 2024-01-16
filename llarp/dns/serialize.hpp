#pragma once

#include <llarp/util/buffer.hpp>
#include <llarp/util/status.hpp>

#include <vector>

namespace llarp::dns
{
  /// base type for serializable dns entities
  struct Serialize
  {
    virtual ~Serialize() = 0;

    /// encode entity to buffer
    virtual bool
    Encode(llarp_buffer_t* buf) const = 0;

    /// decode entity from buffer
    virtual bool
    Decode(llarp_buffer_t* buf) = 0;

    /// convert this whatever into json
    virtual util::StatusObject
    ToJSON() const = 0;
  };

  bool
  EncodeRData(llarp_buffer_t* buf, const std::vector<byte_t>& rdata);

  bool
  DecodeRData(llarp_buffer_t* buf, std::vector<byte_t>& rdata);

}  // namespace llarp::dns
