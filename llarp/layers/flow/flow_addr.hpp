#pragma once

#include <array>
#include <llarp/util/aligned.hpp>
#include <string>
#include <string_view>
#include <type_traits>

#include <oxenc/variant.h>

namespace llarp
{
  struct RouterID;
  namespace service
  {
    struct Address;
  }
}  // namespace llarp

namespace llarp::layers::flow
{

  /// a long form .snode or .loki address
  class FlowAddr : public AlignedBuffer<32>
  {
   public:
    using array_t = AlignedBuffer<32>::Data;
    /// tells us if this flow address is for .loki/.snode/etc
    enum class Kind : uint8_t
    {
      /// not set.
      none,

      /// for .loki
      snapp,
      /// for .snode
      snode,
    };

    FlowAddr();

    /// construct from string
    explicit FlowAddr(std::string str);

    /// address for this kind with raw data.
    FlowAddr(Kind k, array_t data);

    /// construct from decprecated variant.
    explicit FlowAddr(const std::variant<service::Address, RouterID>& arg);

    std::string
    ToString() const;

    bool
    operator==(const FlowAddr& other) const;

    /// tells us what type of flow address it is.
    Kind
    kind() const;

   private:
    static Kind
    get_kind(std::string_view str);

    Kind _kind;
  };

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
    operator()(const llarp::layers::flow::FlowAddr& addr) const
    {
      return std::hash<llarp::AlignedBuffer<32>>{}(addr)
          ^ (std::hash<uint8_t>{}(static_cast<uint8_t>(addr.kind())) << 3);
    }
  };
}  // namespace std
