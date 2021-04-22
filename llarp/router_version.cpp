#include "router_version.hpp"
#include "constants/version.hpp"
#include "constants/proto.hpp"

#include <algorithm>
#include <cassert>

namespace llarp
{
  RouterVersion::RouterVersion(const Version_t& router, uint64_t proto)
      : m_Version(router), m_ProtoVersion(proto)
  {}

  bool
  RouterVersion::IsCompatableWith(const RouterVersion& other) const
  {
    return m_ProtoVersion == other.m_ProtoVersion;
  }

  bool
  RouterVersion::BEncode(llarp_buffer_t* buf) const
  {
    if (not bencode_start_list(buf))
      return false;
    if (not IsEmpty())
    {
      if (not bencode_write_uint64(buf, m_ProtoVersion))
        return false;
      for (const auto& i : m_Version)
      {
        if (not bencode_write_uint64(buf, i))
          return false;
      }
    }
    return bencode_end(buf);
  }

  void
  RouterVersion::Clear()
  {
    m_Version.fill(0);
    m_ProtoVersion = INVALID_VERSION;
    assert(IsEmpty());
  }

  bool
  RouterVersion::IsEmpty() const
  {
    return *this == emptyRouterVersion;
  }

  bool
  RouterVersion::BDecode(llarp_buffer_t* buf)
  {
    // clear before hand
    Clear();
    size_t idx = 0;
    if (not bencode_read_list(
            [self = this, &idx](llarp_buffer_t* buffer, bool has) {
              if (has)
              {
                uint64_t i;
                if (idx == 0)
                {
                  uint64_t val = -1;
                  if (not bencode_read_integer(buffer, &val))
                    return false;
                  self->m_ProtoVersion = val;
                }
                else if (bencode_read_integer(buffer, &i))
                {
                  // prevent overflow (note that idx includes version too)
                  if (idx > self->m_Version.max_size())
                    return false;
                  self->m_Version[idx - 1] = i;
                }
                else
                  return false;
                ++idx;
              }
              return true;
            },
            buf))
      return false;
    // either full list or empty list is valid
    return idx == 4 || idx == 0;
  }

  std::string
  RouterVersion::ToString() const
  {
    return std::to_string(m_Version.at(0)) + "." + std::to_string(m_Version.at(1)) + "."
        + std::to_string(m_Version.at(2)) + " protocol version " + std::to_string(m_ProtoVersion);
  }

}  // namespace llarp
