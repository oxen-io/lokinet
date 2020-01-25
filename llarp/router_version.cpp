#include <router_version.hpp>
#include <constants/version.hpp>
#include <constants/proto.hpp>

#include <algorithm>
#include <cassert>

namespace llarp
{
  RouterVersion::RouterVersion(const std::array< uint64_t, 4 >& other)
  {
    std::copy_n(other.begin(), other.size(), begin());
  }

  bool
  RouterVersion::BEncode(llarp_buffer_t* buf) const
  {
    if(not bencode_start_list(buf))
      return false;
    if(not IsEmpty())
    {
      for(size_t idx = 0; idx < size(); ++idx)
        if(not bencode_write_uint64(buf, at(idx)))
          return false;
    }
    return bencode_end(buf);
  }

  void
  RouterVersion::Clear()
  {
    fill(0);
    at(0) = LLARP_PROTO_VERSION;
    assert(IsEmpty());
  }

  bool
  RouterVersion::IsEmpty() const
  {
    static const RouterVersion empty{{LLARP_PROTO_VERSION, 0, 0, 0}};
    return std::equal(begin(), end(), empty.begin(), empty.end());
  }

  bool
  RouterVersion::BDecode(llarp_buffer_t* buf)
  {
    // clear before hand
    fill(0);
    size_t idx = 0;
    if(not bencode_read_list(
           [self = this, &idx](llarp_buffer_t* buffer, bool has) {
             if(has)
             {
               if(idx >= self->size())
                 return false;
               if(not bencode_read_integer(buffer, &self->at(idx)))
                 return false;
               ++idx;
             }
             return true;
           },
           buf))
      return false;
    // either full list or empty list is valid
    return idx == size() || idx == 0;
  }

  std::string
  RouterVersion::ToString() const
  {
    return std::to_string(at(1)) + "." + std::to_string(at(2)) + "."
        + std::to_string(at(3)) + " protocol version " + std::to_string(at(0));
  }

}  // namespace llarp
