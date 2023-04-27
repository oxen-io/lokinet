#pragma once

#include <oxenc/hex.h>
#include "flow_addr.hpp"
#include "flow_tag.hpp"
#include "flow_constants.hpp"

#include <llarp/constants/bits.hpp>

#include <cstddef>
#include <limits>
#include <llarp/service/convotag.hpp>
#include <utility>
#include <valarray>

namespace llarp::layers::flow
{

  /// information about our flow identity.
  /// in lokinet our onion routed flows are comprised of a source and destination flow layer
  /// address, a flow tag (an identifier to mark a distinct flow), and a pivot that each is using in
  /// common which acts as our analog to an ipv6 flow label, see rfc6437.
  struct FlowInfo
  {
    /// source and destination addresses we associate with this flow.
    FlowAddr src, dst;

    /// to be replaced by a std::optional<FlowTag>.
    /// all flow tags we are using on this flow right now.
    std::set<FlowTag> flow_tags;

    /// mtu between src and dst.
    uint16_t mtu{default_flow_mtu};

    std::string
    ToString() const;

    bool
    operator==(const FlowInfo& other) const;
  };
}  // namespace llarp::layers::flow

namespace llarp
{
  template <>
  inline constexpr bool IsToStringFormattable<llarp::layers::flow::FlowInfo> = true;

}

namespace std
{
  template <>
  struct hash<llarp::layers::flow::FlowInfo>
  {
    static inline constexpr size_t num_hash_bits = llarp::util::num_bits<size_t>();

    // incremenets idx by one and hashes the member
    template <typename T>
    size_t
    hash_member(const T& member, size_t n, size_t prev_hash) const
    {
      size_t h = std::hash<T>{}(member);
      // for the first element just return the hash xor previous hash.
      if (n == 0)
        return h ^ prev_hash;

      auto bytes = std::valarray<bool>(num_hash_bits);
      for (size_t idx = 0; idx < bytes.size(); ++idx)
        bytes[idx] = (h & idx) != 0;

      bytes.cshift(static_cast<int>(n % num_hash_bits));
    }

    size_t
    operator()(const llarp::layers::flow::FlowInfo& info) const
    {
      size_t h{};
      size_t n{};

      h = hash_member(info.src, n++, h);
      h = hash_member(info.dst, n++, h);

      for (const auto& tag : info.flow_tags)
        h = hash_member(tag, n++, h);
      return h;
    }
  };
}  // namespace std
