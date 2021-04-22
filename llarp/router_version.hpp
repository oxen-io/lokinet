#pragma once

#include <array>
#include "util/bencode.hpp"
#include "constants/version.hpp"
#include "constants/proto.hpp"

namespace llarp
{
  struct RouterVersion
  {
    using Version_t = std::array<uint16_t, 3>;

    RouterVersion() = default;

    explicit RouterVersion(const Version_t& routerVersion, uint64_t protoVersion);

    bool
    BEncode(llarp_buffer_t* buf) const;

    bool
    BDecode(llarp_buffer_t* buf);

    /// return true if this router version is all zeros
    bool
    IsEmpty() const;

    /// set to be empty
    void
    Clear();

    std::string
    ToString() const;

    /// return true if the other router version is compatible with ours
    bool
    IsCompatableWith(const RouterVersion& other) const;

    /// compare router versions
    bool
    operator<(const RouterVersion& other) const
    {
      return m_ProtoVersion < other.m_ProtoVersion || m_Version < other.m_Version;
    }

    bool
    operator!=(const RouterVersion& other) const
    {
      return !(*this == other);
    }

    bool
    operator==(const RouterVersion& other) const
    {
      return m_ProtoVersion == other.m_ProtoVersion && m_Version == other.m_Version;
    }

   private:
    Version_t m_Version = {{0, 0, 0}};
    int64_t m_ProtoVersion = LLARP_PROTO_VERSION;
  };

  inline std::ostream&
  operator<<(std::ostream& out, const RouterVersion& rv)
  {
    return out << rv.ToString();
  }

  static constexpr int64_t INVALID_VERSION = -1;
  static const RouterVersion emptyRouterVersion({0, 0, 0}, INVALID_VERSION);

}  // namespace llarp
