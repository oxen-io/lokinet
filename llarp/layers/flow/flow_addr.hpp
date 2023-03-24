#pragma once

#include <fmt/core.h>
#include <llarp/util/aligned.hpp>
#include <string>
#include <string_view>
#include <llarp/endpoint_base.hpp>
#include <type_traits>

namespace llarp::layers::flow
{

  /// a long form .snode or .loki address
  class FlowAddr : public AlignedBuffer<32>
  {
   public:
    enum class Kind : uint8_t
    {
      snapp,
      snode,
    };

    FlowAddr() = default;

    /// construct from string
    explicit FlowAddr(std::string str);

    explicit FlowAddr(EndpointBase::AddressVariant_t arg);

    std::string
    ToString() const;

    bool
    operator==(const FlowAddr& other) const;

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
