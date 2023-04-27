#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <llarp/util/aligned.hpp>
#include <numeric>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include <algorithm>

#include <oxenc/variant.h>

#include <llarp/service/address.hpp>
#include <variant>
#include "llarp/router_id.hpp"
#include "llarp/util/types.hpp"

namespace llarp::layers::flow
{

  /// a long form .snode or .loki address
  class FlowAddr
  {
   public:
    /// the size of the array holding raw address data.
    static constexpr std::size_t size = 32;
    using backing_array_t = AlignedBuffer<size>;

    /// tells us if this flow address is for .loki/.snode/etc
    enum class Kind : uint8_t
    {
      /// monostate
      empty,
      /// for .loki
      snapp,
      /// for .snode
      snode,
    };

    FlowAddr() : FlowAddr{Kind::empty} {};

    /// construct from string
    explicit FlowAddr(std::string str);

    /// make an emtpy flow addr.
    explicit FlowAddr(Kind k);

    /// address for this kind with raw data.
    FlowAddr(Kind k, const backing_array_t& data);

    FlowAddr(const FlowAddr&) = default;

    FlowAddr(FlowAddr&& other);

    FlowAddr&
    operator=(const FlowAddr& other) = default;

    FlowAddr&
    operator=(FlowAddr&& other) &;

    std::string
    ToString() const;

    constexpr bool
    operator==(const FlowAddr& other) const
    {
      return _kind == other._kind and _data == other._data;
    }

    constexpr auto
    kind() const
    {
      return _kind;
    }

    explicit constexpr
    operator backing_array_t&() &
    {
      return _data;
    }

    explicit constexpr operator const backing_array_t&() const&
    {
      return _data;
    }

    constexpr std::array<byte_t, size>&
    as_array()
    {
      return _data.as_array();
    }
    constexpr const std::array<byte_t, size>&
    as_array() const
    {
      return _data.as_array();
    }

    constexpr operator bool() const
    {
      return _kind != Kind::empty;
    }

   private:
    static Kind
    get_kind(std::string_view str);

    Kind _kind;
    backing_array_t _data;
  };

  /// convert to flow addr
  template <typename T>
  static FlowAddr
  to_flow_addr(const T& addr)
  {
    if constexpr (std::is_same_v<T, RouterID>)
      return FlowAddr{FlowAddr::Kind::snode, addr};
    else if constexpr (std::is_same_v<T, service::Address>)
      return FlowAddr{FlowAddr::Kind::snapp, addr};
    else if constexpr (std::is_same_v<T, std::variant<RouterID, service::Address>>)
      return var::visit([](auto&& addr) { return to_flow_addr(addr); }, addr);
    else
      return FlowAddr{FlowAddr::Kind::empty};
  }

}  // namespace llarp::layers::flow

namespace llarp
{
  template <>
  inline constexpr bool IsToStringFormattable<llarp::layers::flow::FlowAddr> = true;

}

namespace std
{

  template <>
  struct hash<llarp::layers::flow::FlowAddr>
  {
    size_t
    operator()(const llarp::layers::flow::FlowAddr& addr) const noexcept
    {
      using backing_t = std::decay_t<decltype(addr)>::backing_array_t;
      return std::hash<backing_t>{}(reinterpret_cast<const backing_t&>(addr))
          ^ (std::hash<uint8_t>{}(static_cast<uint8_t>(addr.kind())) << 3);
    }
  };
}  // namespace std
